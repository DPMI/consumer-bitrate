#ifndef HTTP_H
#define HTTP_H

#include "extract.hpp"
#include <curl/curl.h>

class HTTPOutput {
public:
	HTTPOutput(const char* curl, const char* user, const char* pass);
	~HTTPOutput();

	/**
	 * Perform a HTTP POST request with the provided data as body.
	 *
	 * @param data POST body
	 * @return HTTP status code
	 */
	int POST(const char* data);

protected:
	CURL* curl;
};

#endif /* HTTP_H */
