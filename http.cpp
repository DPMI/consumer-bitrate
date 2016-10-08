#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "http.hpp"

HTTPOutput::HTTPOutput(const char* url, const char* user, const char* pass)
	: curl(nullptr) {

	curl = curl_easy_init();
	if ( curl ){
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_USERNAME, user);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, pass);
	} else {
		fprintf(stdout, "HTTP output, but to stdout.\n");
	}
}

HTTPOutput::~HTTPOutput(){
	if ( curl ){
		curl_easy_cleanup(curl);
	}
}

void HTTPOutput::POST(const char* data){
	if ( !curl ){
		fprintf(stdout, "POST %s\n", data);
		return;
	}

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	CURLcode res = curl_easy_perform(curl);

	/* Check for errors */
	if(res != CURLE_OK){
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	} else {
		fprintf(stderr, "curl OK\n");
	}
}
