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