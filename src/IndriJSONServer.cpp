// #include "IndriPruneIndex.h"

#include <algorithm>
#include <unordered_map>
#include <sstream>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/format.hpp>

// cpp-netlib
#include <boost/network/protocol/http/server.hpp>
#include <boost/network/uri.hpp>

// FIXME: check if this quick fix is appropriate
#define P_NEEDS_GNU_CXX_NAMESPACE 1

#include "indri/Parameters.hpp"
#include "indri/QueryEnvironment.hpp"
#include "indri/SnippetBuilder.hpp"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

//--------------------------------------------------
// Helper functions
//-------------------------------------------------- 
void warn(const char* message) {
    std::cerr << message << "\n";
}

void warn(boost::format& fmt) {
    std::cerr << fmt << "\n";
}

void requires(indri::api::Parameters& parameters, 
	      const std::vector<std::string>& keys) {
    bool found = false;

    BOOST_FOREACH (const std::string& key, keys) {
	if (!parameters.exists(key)) {
	    found = true;
	    warn(boost::format("Parameter '%s' is required") % key);
	}
    }

    if (found) exit(1);
}

void requires(indri::api::Parameters& parameters, const char* key) {
    std::vector<std::string> keys = { key };
    requires(parameters, keys);
}

void parseSpec(indri::api::Parameters& converted, const std::string& spec) {
    int nextComma = 0, nextColon = 0;

    for (int location = 0; location < spec.length();) {
	nextComma = spec.find(',', location);
	nextColon = spec.find(':', location);

	std::string key = spec.substr(location, nextColon-location);
	std::string value = spec.substr(nextColon+1, nextComma-nextColon-1);
	converted.set(key, value);

	if (nextComma > 0) location = nextComma+1;
	else location = spec.size();
    }
}


inline int hex2int(char c) {
    if (c >= '0' && c <= '9') 
	return c - '0';
    else if (c >= 'a' && c <= 'f')
	return 10 + c - 'a';
    else if (c >= 'A' && c <= 'F')
	return 10 + c - 'A';
    else
	return -1;
}

std::string decode(const std::string& s) {
    std::string r;

    for (std::string::const_iterator iter = s.begin();
	 iter != s.end(); )
    {
	if (*iter == '+') {
	    r.push_back(' ');
	    ++iter;
	    continue;
	} 
	
	if (*iter != '%') {
	    r.push_back(*iter);
	    ++iter;
	    continue;
	}
	    
	int i = hex2int(*(iter + 1));
	if (i < 0) {
	    r.push_back(*iter);
	    r.push_back(*(iter + 1));
	    iter += 2;
	    continue;
	}

	int j = hex2int(*(iter + 2));
	if (j < 0) {
	    r.push_back(*iter);
	    r.push_back(*(iter + 1));
	    r.push_back(*(iter + 2));
	    iter += 3;
	    continue;
	}

	char c = 16 * i + j;
	r.push_back(c);
	iter += 3;
    }

    return r;
}

std::string escapeDoubleQuote(const std::string& s) {
    std::string r;

    for (std::string::const_iterator iter = s.begin();
	 iter != s.end(); ++iter)
    {
	if (*iter == '"') r += "\\\"";
	else r.push_back(*iter);
    }

    return r;
}

void parseURIQuery(std::unordered_map<std::string, std::string>& args, 
		   const std::string& query) {
    int nextEqual = 0, nextAmpersand = 0;

    for (int offset = 0; offset < query.size(); ) {
	nextAmpersand = query.find('&', offset);
	nextEqual = query.find('=', offset);

	std::string key = query.substr(offset, nextEqual - offset);
	std::string value = query.substr(nextEqual + 1, nextAmpersand - nextEqual - 1);
	args[key] = decode(value);

	if (nextAmpersand > 0) offset = nextAmpersand + 1;
	else offset = query.size();
    }
}

//--------------------------------------------------
// Main program
//-------------------------------------------------- 
int main(int argc, char** argv) {
    indri::api::Parameters parameters;

    try {
	parameters.loadCommandLine(argc, argv);
    }
    catch (lemur::api::Exception& e) {
	LEMUR_ABORT(e);
    }

    requires(parameters, "index");

    string indexPath = parameters["index"];
    string address = parameters.get("address", "0.0.0.0");
    string port = parameters.get("port", "8000");

    // create the handler
    namespace http = boost::network::http;
    namespace uri = boost::network::uri;

    struct RunQueryHandler;
    typedef http::server<RunQueryHandler> server;

    struct RunQueryHandler {
	indri::api::QueryEnvironment& env;

	RunQueryHandler(indri::api::QueryEnvironment& env): env(env) {}

	void operator () (server::request const &request,
			  server::response &response) {
	    std::unordered_map<std::string, std::string> args;
	    parseURIQuery(args, uri::uri("http://localhost" + request.destination).query());

	    std::string query = args["q"];
	    int count = 50;

	    if (args.find("n") != args.end()) 
		count = boost::lexical_cast<int>(args["n"]);

	    std::string data;
	    std::ostringstream out(data);

	    std::unique_ptr<indri::api::QueryAnnotation> anno(env.runAnnotatedQuery(query, count)); 
	    std::vector<indri::api::ScoredExtentResult> results = anno->getResults();
	    std::vector<indri::api::ParsedDocument*> parsedDocs = env.documents(results);
	    std::vector<std::string> docNames;

	    for (int i = 0; i < parsedDocs.size(); ++i) {
		indri::utility::greedy_vector<indri::parse::MetadataPair>::iterator iter = 
		    std::find_if(parsedDocs[i]->metadata.begin(),
				 parsedDocs[i]->metadata.end(),
				 indri::parse::MetadataPair::key_equal("docno"));
		docNames.push_back("-NA-");
		if (iter != parsedDocs[i]->metadata.end()) 
		    docNames.back() = (char*)iter->value;
	    }

	    out << "{\"query\":\"" << escapeDoubleQuote(query) << "\",\"count\":" << count << ",\"result\":[";
	    for (int i = 0; i < results.size(); ++i) {
		if (i > 0) out << ",";
		out << "{\"score\":" << results[i].score << ','
		    << "\"docno\":\"" << docNames[i] << "\","
		    << "\"begin\":" << results[i].begin << ','
		    << "\"end\":" << results[i].end << "}";
	    }
	    out << "]}\n";

	    response = server::response::stock_reply(server::response::ok, out.str());
	}

	void log (const std::string& what) {}
    };

    indri::api::QueryEnvironment env;
    env.addIndex(indexPath);

    try {
        RunQueryHandler handler(env);
	server::options options(handler);
	server server_(options.address(address).port(port));
	server_.run();
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
