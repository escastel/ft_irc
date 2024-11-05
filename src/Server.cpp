#include <Server.hpp>

void Server::createServerSocket() {
    // Crear el socket del servidor
    this->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (this->serverSocket == -1) print_err("Unable to create server socket");

    int optval = 1;
    if (setsockopt(this->serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval,
                   sizeof(optval)) == -1)
        print_err("Unable to set options to server socket");

    // Especificaciones de la dirección
    this->serverAddress.sin_family = AF_INET;
    this->serverAddress.sin_port = htons(this->port);
    this->serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Hace que se repita el proceso
    if (fcntl(serverSocket, F_SETFL, O_NONBLOCK) == -1)
        print_err("Unable to set server socket as non blocking");

    // Vincula el fd del servidor a una dirección
    if (bind(this->serverSocket, (struct sockaddr*)&serverAddress,
             sizeof(serverAddress)))
        print_err("Unable to bind server socket");

    // Permite que el servidor escuche conexiones
    if (listen(serverSocket, 5) == -1)
        print_err("Unable to listen server socket");

    struct pollfd newPoll;

    newPoll.fd = this->serverSocket;
    newPoll.events = POLLIN;
    newPoll.revents = 0;
    fds.push_back(newPoll);
}

static int parsePort(std::string port) {
    if (port.find_first_not_of("0123456789") != std::string::npos)
        print_err("Port number is invalid");
    return (atoi(port.c_str()));
}

static std::string parsePassword(std::string password) {
    for (unsigned long int i = 0; i < password.size(); i++) {
        if (!isalnum(password[i]))
            print_err("Password only accepts alphanumeric characters");
    }
    return (password);
}

Server::Server(std::string port, std::string password) {
    ft_print(ASCII_ART);
    ft_print("IRC CONSTRUCTOR CALLED");
    this->port = parsePort(port);
    this->password = parsePassword(password);

    createServerSocket();
}

Server::~Server() { close(this->serverSocket); }

int Server::getServerSocket(void) const { return (this->serverSocket); }
int Server::getPort(void) const { return (this->port); }
std::string Server::getPassword(void) const { return (this->password); }
std::vector<Client> Server::getClients(void) const { return (this->clients); }

int Server::checkConnections(void) {
    int result = poll(&this->fds[0], fds.size(), 0);
    if (result == -1) print_err("Error while trying to poll()");
    return (result);
}

int Server::iterateFds(void) {
    unsigned long int i = 0;

    for (; i < this->fds.size(); i++) {
        // Miramos si hay respuesta y esa respuesta es POLLIN (leer como suena
        // c=3)
        if (fds[i].revents & POLLIN) {
            if (fds[i].fd == this->serverSocket) this->newConnection();
            // else
            //  	this->manageUpdates();
        }
    }
    return (i);
}

void Server::newConnection() {
    sockaddr_in address;
    socklen_t length = sizeof(address);

    int clientSocket =
        accept(this->getServerSocket(), (sockaddr*)&address, &length);
    if (clientSocket == -1) exit(1);
    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) == -1) exit(1);

    Client client(clientSocket, inet_ntoa(address.sin_addr));
    this->clients.push_back(client);

    pollfd poll;
    poll.fd = clientSocket;
    poll.events = POLLIN;
    poll.revents = 0;
    this->fds.push_back(poll);

    // this->manageUpdates();
    std::cout << client << std::endl;
}

std::ostream& operator<<(std::ostream& os, const Server& server) {
    os << "* Server information *";
    os << "\nPassword: " << server.getPassword();
    os << "\nPort: " << server.getPort();
    os << "\nSocket: " << server.getServerSocket();
    os << "\nCurrent amount of clients: " << server.getClients().size();
    return (os);
}