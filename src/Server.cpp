/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: albagar4 <albagar4@student.42malaga.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:30:32 by albagar4          #+#    #+#             */
/*   Updated: 2024/11/27 11:30:41 by albagar4         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <Channel.hpp>
#include <Server.hpp>
#include <iterator>

void Server::createServerSocket() {
    this->serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (this->serverSocket == -1) print_err("Unable to create server socket");

    int optval = 1;
    if (setsockopt(this->serverSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1)
        print_err("Unable to set options to server socket");

    this->serverAddress.sin_family = AF_INET;
    this->serverAddress.sin_port = htons(this->port);
    this->serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (fcntl(serverSocket, F_SETFL, O_NONBLOCK) == -1)
        print_err("Unable to set server socket as non blocking");

    if (bind(this->serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)))
        print_err("Unable to bind server socket");

    if (listen(serverSocket, 5) == -1) print_err("Unable to listen server socket");

    struct pollfd newPoll;

    newPoll.fd = this->serverSocket;
    newPoll.events = POLLIN;
    newPoll.revents = 0;
    fds.push_back(newPoll);
    char buffer[1024];
    if (gethostname(buffer, 1024) == -1) {
        print_err("Unable to fetch hostname");
    }
    this->hostname = buffer;
}

static int parsePort(std::string port) {
    if (port.find_first_not_of("0123456789") != std::string::npos)
        print_err("Port number is invalid");
    return (atoi(port.c_str()));
}

static std::string parsePassword(std::string password) {
    for (unsigned long int i = 0; i < password.size(); i++) {
        if (!isalnum(password[i])) print_err("Password only accepts alphanumeric characters");
    }
    return (password);
}

Server::Server(std::string port, std::string password) {
    this->port = parsePort(port);
    this->password = parsePassword(password);
    this->setAuthFunctions();
    this->setCmdFunctions();
    createServerSocket();
    printLaunchServer(*this);
}

Server::~Server() { close(this->serverSocket); }

void Server::setAuthFunctions(void) {
    this->authentification[0] = &Server::parseCap;
    this->authentification[1] = &Server::parsePass;
    this->authentification[2] = &Server::parseNick;
    this->authentification[3] = &Server::parseUser;
    this->authentification[4] = &Server::parseQuit;
}

void Server::setCmdFunctions(void) {
    this->commands[0] = &Server::parseJoin;
    this->commands[1] = &Server::parsePart;
    this->commands[2] = &Server::parseKick;
    this->commands[3] = &Server::parseInvite;
    this->commands[4] = &Server::parseTopic;
    this->commands[5] = &Server::parseMode;
    this->commands[6] = &Server::parsePrivMsg;
    this->commands[7] = &Server::parseQuit;
    this->commands[8] = &Server::parseNick;
}

int Server::getServerSocket(void) const { return (this->serverSocket); }
int Server::getPort(void) const { return (this->port); }
std::string Server::getHostname() const { return (this->hostname); }
std::string Server::getPassword(void) const { return (this->password); }
std::map<int, Client>& Server::getMap(void) { return (this->map); }
std::vector<File> Server::getFiles(void) const { return (this->files); }
std::vector<Channel>& Server::getChannels() { return (this->channels); }

int Server::checkConnections(void) {
    int result = poll(&this->fds[0], fds.size(), 0);
    if (result == -1 && !g_signal) print_err("Error while trying to poll()");
    return (result);
}

int Server::iterateFds(void) {
    unsigned long int i = 0;

    for (; i < this->fds.size(); i++) {
        if (fds[i].revents & POLLIN) {
            if (fds[i].fd == this->serverSocket)
                this->newConnection();
            else
                this->manageUpdates(this->map[fds[i].fd]);
        }
    }
    return (i);
}

void Server::newConnection() {
    sockaddr_in address;
    socklen_t length = sizeof(address);

    int clientSocket = accept(this->getServerSocket(), (sockaddr*)&address, &length);
    if (clientSocket == -1) exit(1);
    if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) == -1) exit(1);

    Client client(clientSocket, address);
    this->map[clientSocket] = client;

    pollfd poll;
    poll.fd = clientSocket;
    poll.events = POLLIN;
    poll.revents = 0;
    this->fds.push_back(poll);
    printConnected(client);
}

void Server::manageUpdates(Client& client) {
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes = recv(client.getFd(), buffer, sizeof(buffer) - 1, 0);
    if (bytes == -1) print_err("recv has failed");
    if (bytes == 0) {
        this->disconnectClient(client);
    }
    if (bytes > 0) {
        std::string message(buffer);
        if (message.find_first_of("\n") != std::string::npos) {
            client.setIncomingMessage(client.getIncomingMessage() + message);
            int clientFd = client.getFd();
            this->parseCommands(client.getIncomingMessage(), client);
            if (this->map.find(clientFd) != this->map.end()) client.setIncomingMessage("");
        } else {
            client.setIncomingMessage(client.getIncomingMessage() + message);
        }
    }
}

void Server::findCommand(std::string buffer, Client& client) {
    if (!buffer.empty() && buffer[buffer.size() - 1] == '\n') {
        buffer.erase(buffer.size() - 1);
    }
    if (!buffer.empty() && buffer[buffer.size() - 1] == '\r') {
        buffer.erase(buffer.size() - 1);
    }
    std::string title = buffer.substr(0, buffer.find(" "));
    if (client.getAuth() == false) {
        std::string commandArray[5] = {"CAP", "PASS", "NICK", "USER", "QUIT"};
        for (int j = 0; j < 5; j++) {
            if (title == commandArray[j]) {
                (this->*authentification[j])(fixSpaces(buffer), client);
                return;
            }
        }
    } else {
        std::string commandArray[9] = {"JOIN", "PART",    "KICK", "INVITE", "TOPIC",
                                       "MODE", "PRIVMSG", "QUIT", "NICK"};
        for (int j = 0; j < 9; j++) {
            if (title == commandArray[j]) {
                (this->*commands[j])(fixSpaces(buffer), client);
                return;
            }
        }
    }
    err(ERR_UNKNOWNCOMMAND, this->getHostname(), client, buffer);
}

void Server::sendResponse() {
    std::map<int, Client>::iterator it = map.begin();
    std::advance(it, 4);
    for (; it != this->map.end(); it++) {
        if (!it->second.getResponse().empty()) {
            send(it->first, it->second.getResponse().c_str(), it->second.getResponse().size(), 0);
            it->second.setResponse("");
        }
    }
}

void Server::parseCommands(std::string buff, Client& client) {
    std::vector<std::string> lines = split(buff, '\n');

    for (size_t i = 0; i < lines.size(); i++) {
        findCommand(lines[i], client);
        sendResponse();
    }
}

void Server::removeAllInvited(Client& client) {
    std::vector<Channel>::iterator it = this->getChannels().begin();
    for (; it != channels.end(); it++) {
        std::vector<Client> invitedClients = it->getInviteds();
        std::vector<Client>::iterator itClients = invitedClients.begin();
        for (; itClients != invitedClients.end(); itClients++) {
            if (itClients->getFd() == client.getFd()) it->removeInvited(client);
        }
    }
}

void Server::disconnectClient(Client& client) {
    Client temp = client;
    for (std::vector<pollfd>::iterator it = this->fds.begin(); it != this->fds.end(); it++) {
        if (temp.getFd() == it->fd) {
            std::vector<Channel*> channels = this->findUserChannels(temp);
            for (size_t i = 0; i < channels.size(); i++) {
                channels[i]->updateClients(
                    temp, ":" + temp.getHostname() + " PART " + channels[i]->getName() + "\r\n");
                channels[i]->removeInvited(client);
                channels[i]->disconnectClient(client);
                if (channels[i]->isEmpty()) this->closeChannel(*channels[i]);
                if (channels[i]->getOperators().empty() && !channels[i]->getClients().empty()) {
                    std::vector<Client>& clients = channels[i]->getClients();
                    Client& newOperator = clients[0];
                    channels[i]->addOperator(newOperator);
                    channels[i]->updateClients(
                        temp, newOperator.getResponse() + ":" + this->hostname + " MODE " +
                                  channels[i]->getName() + " +o " + newOperator.getNick() + "\r\n");
                }
            }
            this->removeAllInvited(client);
            this->fds.erase(it);
            this->map.erase(temp.getFd());
            close(temp.getFd());
            printDisconnected(temp);
            sendResponse();
            break;
        }
    }
}

void Server::printChannels() {
    ft_print("The current channels are:");
    for (size_t i = 0; i < this->channels.size(); i++) {
        std::cout << this->channels[i].getName();
        std::cout << " :" << this->channels[i].getTopic();
        std::cout << " (";
        this->channels[i].printClients();
        std::cout << ")" << std::endl;
    }
}
bool Server::isNewChannel(std::string name) {
    for (size_t i = 0; i < this->channels.size(); i++) {
        if (this->channels[i].getName() == name) return false;
    }
    return true;
}

Channel* Server::findChannel(std::string name) {
    for (size_t i = 0; i < this->channels.size(); i++) {
        if (this->channels[i].getName() == name) return &this->channels[i];
    }
    return NULL;
}
Client* Server::findClient(std::string name) {
    for (std::map<int, Client>::iterator it = this->map.begin(); it != this->map.end(); it++) {
        if (it->second.getNick() == name) return &it->second;
    }
    return NULL;
}
void Server::closeChannel(Channel channel) {
    for (size_t i = 0; i < this->channels.size(); i++) {
        if (this->channels[i].getName() == channel.getName()) {
            this->channels.erase(this->channels.begin() + i);
            return;
        }
    }
}
std::vector<Channel*> Server::findUserChannels(Client& client) {
    std::vector<Channel*> activeChannels;
    std::vector<Channel>& channels = this->getChannels();
    std::vector<Channel>::iterator it = channels.begin();
    for (; it != channels.end(); it++) {
        if (it->isClient(client)) activeChannels.push_back(&(*it));
    }
    return activeChannels;
}
