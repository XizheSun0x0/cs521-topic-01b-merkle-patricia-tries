//server.cpp — A simple POSIX socket server for handling HTTP requests
//This server provides a RESTful API to interact with the Merkle Patricia Trie. It supports operations to insert, retrieve, delete key-value pairs, and generate/verify Merkle proofs. The trie structure can also be visualized as JSON.   
//Note: This server is intended for demonstration and testing purposes. It is not optimized for performance or security and should not be used in production environments.
#define MPT_IMPLEMENTATION
#include "merkle_patricia_trie.h"

#include <map>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <cstdlib>
// ═══════════════════════════════════════════════════════════════════════════════
//  Global trie instance + entry tracking
// ═══════════════════════════════════════════════════════════════════════════════
static MerklePatriciaTrie g_trie;
static std::map<std::string, std::string> g_entries;

// ═══════════════════════════════════════════════════════════════════════════════
//  JSON helpers (no library needed)
// ═══════════════════════════════════════════════════════════════════════════════
static std::string json_escape(const std::string &s){
    std::string out;
    // Reserve enough space to avoid multiple reallocations
    out.reserve(s.size());
    // Escape special characters for JSON strings
    for (size_t i = 0; i < s.size(); i++)
    {
        char c = s[i];
        if (c == '"')
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\n')
            out += "\\n";
        else if (c == '\t')
            out += "\\t";
        else
            out += c;
    }
    return out;
}

// Extracts a string value from a JSON object given a key. This is a very basic implementation and does not handle all edge cases or JSON formats. It assumes the JSON is well-formed and that the value is a string enclosed in double quotes.
static std::string json_get_string(const std::string &json, const std::string &key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos)
        return "";
    pos = json.find(":", pos + search.size());
    if (pos == std::string::npos)
        return "";
    pos = json.find("\"", pos + 1);
    if (pos == std::string::npos)
        return "";
    size_t end = pos + 1;
    while (end < json.size() && !(json[end] == '"' && json[end - 1] != '\\'))
        end++;
    if (end >= json.size())
        return "";
    return json.substr(pos + 1, end - pos - 1);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Tree-to-JSON serializer (recursive)
// ═══════════════════════════════════════════════════════════════════════════════
static void node_to_json(const NodePtr &node, std::ostringstream &ss)
{
    if (!node || node->type == NODE_NULL)
    {
        ss << "null";
        return;
    }
    ss << "{\"id\":" << node->node_id << ",";
    switch (node->type)
    {
    case NODE_LEAF:
        ss << "\"type\":\"LEAF\",\"partial\":\"";
        for (size_t i = 0; i < node->partial.size(); i++)
            ss << std::hex << (int)node->partial[i];
        ss << std::dec << "\",\"value\":\"" << json_escape(std::string(node->value.begin(), node->value.end())) << "\"";
        break;
    case NODE_EXTENSION:
        ss << "\"type\":\"EXT\",\"partial\":\"";
        for (size_t i = 0; i < node->partial.size(); i++)
            ss << std::hex << (int)node->partial[i];
        ss << std::dec << "\",\"child\":";
        node_to_json(node->next, ss);
        break;
    case NODE_BRANCH:
        ss << "\"type\":\"BRANCH\",\"children\":[";
        for (int i = 0; i < 16; i++)
        {
            if (i > 0)
                ss << ",";
            if (node->children[i] && node->children[i]->type != NODE_NULL)
            {
                ss << "{\"nibble\":" << i << ",\"node\":";
                node_to_json(node->children[i], ss);
                ss << "}";
            }
            else
                ss << "null";
        }
        ss << "]";
        if (!node->branch_value.empty())
            ss << ",\"value\":\"" << json_escape(std::string(node->branch_value.begin(), node->branch_value.end())) << "\"";
        break;
    default:
        break;
    }
    ss << "}";
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HTTP parser
// ═══════════════════════════════════════════════════════════════════════════════
static void parse_http(const std::string &raw, std::string &method, std::string &path, std::string &body)
{
    size_t le = raw.find("\r\n");
    if (le == std::string::npos)
    {
        method = path = body = "";
        return;
    }
    // Parse the request line (e.g., "GET /path HTTP/1.1")
    std::string line = raw.substr(0, le);
    // Split the request line into method, path, and ignore the HTTP version
    size_t s1 = line.find(' '), s2 = line.find(' ', s1 + 1);
    // Extract the HTTP method and path
    method = line.substr(0, s1);
    // Extract the path from the request line
    path = line.substr(s1 + 1, s2 - s1 - 1);
    // Find the start of the body (after the header section)
    size_t bs = raw.find("\r\n\r\n");
    // Extract the body if it exists, otherwise set it to an empty string
    body = (bs != std::string::npos) ? raw.substr(bs + 4) : "";
}

// ═══════════════════════════════════════════════════════════════════════════════
//  HTTP response builder
// ═══════════════════════════════════════════════════════════════════════════════
static std::string http_response(int code, const std::string &body)
{
    const char *status = "200 OK";
    if (code == 400)
        status = "400 Bad Request";
    else if (code == 404)
        status = "404 Not Found";
    std::ostringstream ss;
    ss << "HTTP/1.1 " << status << "\r\n"
       << "Content-Type: application/json\r\n"
       << "Access-Control-Allow-Origin: *\r\n"
       << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
       << "Access-Control-Allow-Headers: Content-Type\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n\r\n"
       << body;
    return ss.str();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Main — POSIX socket server
// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char *argv[])
{
    int port = 8080;
    if (argc > 1)
        port = std::atoi(argv[1]);

    signal(SIGPIPE, SIG_IGN);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0)
    {
        perror("socket");
        return 1;
    }
    int opt = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(sfd);
        return 1;
    }
    if (listen(sfd, 16) < 0)
    {
        perror("listen");
        close(sfd);
        return 1;
    }
    std::cout << "\n"
              << "  ┌───────────────────────────────────────────────┐\n"
              << "  │  MPT Server: http://localhost:" << port << "              │\n"
              << "  │  Ctrl+C to stop                              │\n"
              << "  └───────────────────────────────────────────────┘\n\n";

    while (true){}
    close(sfd);
    return 0;
}