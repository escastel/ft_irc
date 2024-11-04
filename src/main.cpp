#include <ircserv.hpp>
#include <Server.hpp>

int main(int argc, char **argv) {
  if (argc != 3) {
    ft_print("Incorrect nbr of parameters");
    ft_print("Usage: ./ircserv <port> <password>");
  }
  else {
    Server server(argv[1], argv[2]);

    while (true) {
      if (server.checkConnections() > 0) {
        int clientSocket = accept(server.getServerSocket(), 0, 0);
        char buffer[1024] = { 0 };
        if (recv(clientSocket, buffer, sizeof(buffer), 0)) {
          ft_print("Message from client: ");
          ft_print(buffer);
      }
      }
      // poll
      // checkeamos clientes y mensaje
      // ejecutamos comando (?)
    }
  }
  return 0;
}
