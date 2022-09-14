#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <arpa/inet.h>
#include <fstream>

using namespace std;
using std::cout;
using std::endl;
using std::string;
using std::stringstream;

#define PORT "80"

#define MAXDATASIZE 100000

class Url
{
    std::string protocol;
    std::string domain;
    std::string port;
    std::string path;
    std::string query;

public:
    Url(std::string protocol, std::string domain, std::string port, std::string path, std::string query)
    {
        this->protocol = protocol;
        this->domain = domain;
        this->port = port;
        this->path = path;
        this->query = query;
    }

    std::string getHost()
    {
        return this->domain;
    }

    std::string getPort()
    {
        return this->port.empty() ? PORT : this->port;
    }

    std::string getPath()
    {
        return (this->path.empty()) ? "" : (this->query.empty()) ? this->path
                                                                 : this->path + "?" + this->query;
    }
};

std::string _trim(const string &str)
{
    size_t start = str.find_first_not_of(" \n\r\t");
    size_t until = str.find_last_not_of(" \n\r\t");
    string::const_iterator i = start == string::npos ? str.begin() : str.begin() + start;
    string::const_iterator x = until == string::npos ? str.end() : str.begin() + until + 1;
    return string(i, x);
}

Url parse_url(const string &raw_url) 
{
    string path, domain, x, protocol, port, query;
    int offset = 0;
    size_t pos1, pos2, pos3, pos4;
    x = _trim(raw_url);
    offset = offset == 0 && x.compare(0, 8, "https://") == 0 ? 8 : offset;
    offset = offset == 0 && x.compare(0, 7, "http://") == 0 ? 7 : offset;
    pos1 = x.find_first_of('/', offset + 1);
    path = pos1 == string::npos ? "" : x.substr(pos1);
    domain = string(x.begin() + offset, pos1 != string::npos ? x.begin() + pos1 : x.end());
    path = (pos2 = path.find("#")) != string::npos ? path.substr(0, pos2) : path;
    port = (pos3 = domain.find(":")) != string::npos ? domain.substr(pos3 + 1) : "";
    domain = domain.substr(0, pos3 != string::npos ? pos3 : domain.length());
    protocol = offset > 0 ? x.substr(0, offset - 3) : "";
    query = (pos4 = path.find("?")) != string::npos ? path.substr(pos4 + 1) : "";
    path = pos4 != string::npos ? path.substr(0, pos4) : path;
    Url url(protocol, domain, port, path, query);
    return url;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    ofstream MyFile("output");

    if (argc != 2)
    {
        fprintf(stderr, "usage: ./client http://hostname[:port]/path_to_file\n");
        exit(1);
    }

    string input = argv[1];
    if (!(input.rfind("http", 0) == 0 || input.rfind("https", 0) == 0))
    {
        MyFile << "INVALIDPROTOCOL";
        MyFile.close();
        return 1;
    }

    Url url = parse_url(input);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(url.getHost().c_str(), url.getPort().c_str(), &hints, &servinfo)) != 0)
    {
        MyFile << "NOCONNECTION";
        MyFile.close();
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        MyFile << "NOCONNECTION";
        MyFile.close();
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);

    freeaddrinfo(servinfo);

    std::string request = "GET " + url.getPath() + " HTTP/1.1\r\nHost: " + url.getHost() + "\r\nConnection: close\r\n\r\n";

    if (send(sockfd, request.c_str(), request.size(), 0) < 0)
    {
        close(sockfd);
        return 0;
    }

    int n;
    std::string raw_site;
    while ((n = recv(sockfd, buf, sizeof(buf), 0)) > 0)
    {
        raw_site.append(buf, n);
    }

    close(sockfd);

    int found = raw_site.find("\r\n\r\n");
    string header = raw_site.substr(0, found + 1);
    string line1 = header.substr(9);
    string statusCode = line1.substr(0, 3);

    if (statusCode == "200")
    {
        string body = raw_site.substr(found + 4);
        MyFile << body;
    }
    else if (statusCode == "404")
    {
        MyFile << "FILENOTFOUND";
    }

    MyFile.close();
    return 0;
}