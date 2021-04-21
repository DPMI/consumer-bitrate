#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "http.hpp"

static size_t write_chunk(void* buffer, size_t size, size_t nmemb, void* userp){
	/* do nothing */
	return size * nmemb;
}

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

int HTTPOutput::POST(const char* data){
	if ( !curl ){
		fprintf(stdout, "POST %s\n", data);
		return 200;
	}

	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_chunk);
	const CURLcode res = curl_easy_perform(curl);

	/* check for errors */
	if ( res != CURLE_OK ){
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		return 500;
	}

	/* return HTTP status code */
	long response;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
	return response;
}

