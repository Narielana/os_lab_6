#include <iostream>
#include <unistd.h>
#include <vector>
#include <algorithm>
#include <string>
#include <csignal>
#include <signal.h>
#include <sys/types.h>
#include "wrap_zmq.h"
#include "socket.h"

using namespace std;


class Client {
private:
	int id; // ID
	void* context; // Контекст
	bool terminated; // Переменная работоспособности
public:
	Socket* child_publisher_left;
	Socket* child_publisher_right; 
	Socket* parent_publisher; 
	Socket* parent_subscriber;
	Socket* left_subscriber;
	Socket* right_subscriber; 
	Client(int new_id, string parent_endpoint, int new_parent_id) { // Конструктор клиента
		id = new_id; 
		parent_id = new_parent_id;
		context = create_zmq_ctx(); // Создание контекста
		string endpoint = create_endpoint(EndpointType::CHILD_PUB_LEFT, getpid()); // Создаёт  endpoint
		child_publisher_left = new Socket(context, SocketType::PUBLISHER, endpoint);
		endpoint = create_endpoint(EndpointType::CHILD_PUB_RIGHT, getpid()); // Создаёт  endpoint
		child_publisher_right = new Socket(context, SocketType::PUBLISHER, endpoint); 
		endpoint = create_endpoint(EndpointType::PARENT_PUB, getpid()); // Создаёт endpoint
		parent_publisher = new Socket(context, SocketType::PUBLISHER, endpoint); 
		parent_subscriber = new Socket(context, SocketType::SUBSCRIBER, parent_endpoint); 
		left_subscriber = nullptr;
		right_subscriber = nullptr; 
		terminated = false;
	}
	~Client() { // Деструктор клиента
		if (terminated) {
			return;
		}
		terminated = true;
		try {
			delete child_publisher_left;
			delete child_publisher_right;
			delete parent_publisher;
			delete parent_subscriber;
			if (left_subscriber) {
				delete left_subscriber;
			}
			if (right_subscriber) {
				delete right_subscriber;
			}
			destroy_zmq_ctx(context); // Уничтожение контекста
		} 
		catch (runtime_error& err) {
			cout << "Server wasn't stopped " << err.what() << endl;
		}
	}
	bool& get_status() { // Получение статуса
		return terminated;
	}
	void send_up(Message msg) { // Отправляет сообщение сокету родителя
		msg.to_up = true;
		parent_publisher->send(msg);
	}
	void send_down(Message msg) { // Отправляет сообщение сокету ребёнка
		msg.to_up = false;
		child_publisher_left->send(msg);
		child_publisher_right->send(msg);
	}
	Message receive(); // Получение сообщения
	int get_id() { // Получение id
		return id;
	}
	int add_child(int new_id) { // Добавить ребёнка
		pid_t pid = fork();
		if (pid == -1) {
			throw runtime_error("Can not fork.");
		}
		if (pid == 0) {
			string endpoint;
			if (new_id < id) {
				endpoint = child_publisher_left->get_endpoint();
			} 
			else {
				endpoint = child_publisher_right->get_endpoint();
			}
			execl("client", "client", to_string(new_id).data(), endpoint.data(), to_string(id).data(), nullptr);
			throw runtime_error("Can not execl.");
		}
		string endpoint = create_endpoint(EndpointType::PARENT_PUB, pid);
		size_t timeout = 10000; // Время ожидания ответа
		if (id > new_id) {
			left_subscriber = new Socket(context, SocketType::SUBSCRIBER, endpoint);
			zmq_setsockopt(left_subscriber->get_socket(), ZMQ_RCVTIMEO, &timeout, sizeof(timeout)); // Установка предельного времени ожидания сообщения 
		} 
		else {
			right_subscriber = new Socket(context, SocketType::SUBSCRIBER, endpoint);
			zmq_setsockopt(right_subscriber->get_socket(), ZMQ_RCVTIMEO, &timeout, sizeof(timeout));  // Установка предельного времени ожидания сообщения
		}
		return pid;
	}
	int parent_id; //id родителя
};


void process_msg(Client& client, Message msg) { // Выполнение запроса из сообщения
	switch(msg.command) {
		case CommandType::ERROR: {
			throw runtime_error("Error message received.");
		}
		case CommandType::RETURN: { 
			msg.get_to_id() = SERVER_ID;
			client.send_up(msg);
			break;
		}
		case CommandType::CREATE_CHILD: { // Создать ребёнка
			msg.get_create_id() = client.add_child(msg.get_create_id());
			msg.get_to_id() = SERVER_ID;
			client.send_up(msg);
			break;
		}
		case CommandType::REMOVE_CHILD: { // Удалить ребёнка
			if (msg.to_up) {
				client.send_up(msg);
				break;
			}
			if (msg.to_id != client.get_id() && msg.to_id != UNIVERSAL_MSG) {
				client.send_down(msg);
				break;
			}
			msg.get_to_id() = PARENT_SIGNAL;
			msg.get_create_id() = client.get_id(); 
			client.send_up(msg);
			msg.get_to_id() = UNIVERSAL_MSG;
			client.send_down(msg);
			client.~Client();
			throw invalid_argument("Exiting child...");
			break;
		}
		case CommandType::EXEC_CHILD: { // Исполнение команды на вычислительном узле
			int n = msg.size;
			msg.sum = 0;
			for (int i = 0; i < n; i++) {
				msg.sum += msg.buf[i];
			}
			msg.get_to_id() = SERVER_ID;
			msg.get_create_id() = client.get_id();
			client.send_up(msg);
			break;
		}
		default:
			throw runtime_error("Undefined command.");
	}
}

Client* client_ptr = nullptr;
void TerminateByUser(int) { // Завершение работы клиента
	if (client_ptr != nullptr) {
		client_ptr->~Client();
	}
	cout << to_string(getpid()) + " Terminated by user" << endl;
	exit(0);
}

int main (int argc, char const *argv[]) {
	if (argc != 4) {
		cout << "-1" << endl;
		return -1;
	}
	try {
		if (signal(SIGINT, TerminateByUser) == SIG_ERR) { // Обработка сигналов
			throw runtime_error("Can not set SIGINT signal");
		}
		if (signal(SIGSEGV, TerminateByUser) == SIG_ERR) { // Обработка сигналов
			throw runtime_error("Can not set SIGSEGV signal");
		}
		if (signal(SIGTERM, TerminateByUser) == SIG_ERR) { // Обработка сигналов
			throw runtime_error("Can not set SIGTERM signal");
		}
		Client client(stoi(argv[1]), string(argv[2]), stoi(argv[3])); // Создание клиента
		client_ptr = &client;
		cout << getpid() << ": " "Client started. "  << "Id:" << client.get_id() << endl;
		for (;;) { // Выполнение команд
			Message msg = client.parent_subscriber->receive();
			if (msg.to_id != client.get_id() && msg.to_id != UNIVERSAL_MSG) {
				if (msg.to_up) {
					client.send_up(msg);
				} 
				else {
					try {
						if (client.get_id() < msg.to_id) {
							msg.to_up = false;
							client.child_publisher_right->send(msg);
							msg = client.right_subscriber->receive();
						} 
						else {
							msg.to_up = false;
							client.child_publisher_left->send(msg);
							msg = client.left_subscriber->receive();
						}
						if (msg.command == CommandType::REMOVE_CHILD && msg.to_id == PARENT_SIGNAL) {
							msg.to_id = SERVER_ID;
							if (client.get_id() < msg.get_create_id()) {
								delete client.right_subscriber;
								client.right_subscriber = nullptr;
							} 
							else {
								delete client.left_subscriber;
								client.left_subscriber = nullptr;
							}
						}
						client.send_up(msg);
					}
					catch (...) {
						client.send_up(Message());
					}
				}
			} 
			else {
				process_msg(client, msg);
			}
		}
	} 
	catch(runtime_error& err) {
		cout << getpid() << ": " << err.what() << '\n';
	} 
	catch(invalid_argument& inv) {
		cout << getpid() << ": " << inv.what() << '\n';
	}
	return 0;
}
