#include <vector>
#include <unistd.h>
#include <csignal>
#include <iostream>
#include "socket.h"
#include "wrap_zmq.h"
#include "tree.h"

using namespace std;

#define msg_wait_time 1

void* subscriber_thread(void* server);
void* heartbits_func(void* server);

class Server {
public:
	pid_t pid; // pid сервера
	tree t; // Дерево узлов
	void *context = nullptr; // Контекст
	Socket* publisher; // Сокет для передачи клиенту сообщения
	Socket* subscriber; // Сокет для получения от клиента сообщения
	bool working; // Переменная работоспособности сервера
	pthread_t receive_thread; // Поток для получения сообщения
	pthread_t heartbits_thread;  // Поток для постоянной проверки узлов 
	int heartbit_time; // Время которое надо ждать
	bool is_heartbit; // Переменная для запуска или остановки heartbit
	Server() { // Конструктор сервера
		context = create_zmq_ctx();
		pid = getpid();
		string endpoint = create_endpoint(EndpointType::CHILD_PUB_LEFT, getpid());
		publisher = new Socket(context, SocketType::PUBLISHER, endpoint);
		is_heartbit = false;
		if (pthread_create(&receive_thread, 0, subscriber_thread, this) != 0) {
			throw runtime_error("Can not run second thread.");
		}
		working = true;
	}
	~Server() { // Деструктор сервера
		if (!working) {
			return;
		}
		working = false;
		try {
			send(Message(CommandType::REMOVE_CHILD, 0, 0));
			delete publisher;
			delete subscriber;
			publisher = nullptr;
			subscriber = nullptr;
			destroy_zmq_ctx(context);
			sleep(2);
		} 
		catch (runtime_error &err) {
			cout << "Server wasn't stopped " << err.what() << "\n";
		}
	}
	void send(Message msg) { // Отправка сообщения
		msg.to_up = false;
		publisher->send(msg);
	}
	Message receive_msg() { // Получение сообщения от ребёнка
		return subscriber->receive();
	}
	void create_child(int id) { // Создать дочерний узел
		if (t.find(id)) {
			throw runtime_error("Error:" + to_string(id) + ":Node with that number already exists.");
		}
		if (t.get_place(id) && !check(t.get_place(id))) {
			throw runtime_error("Error:" + to_string(id) + ":Parent node is unavailable.");
		}
		send(Message(CommandType::CREATE_CHILD, t.get_place(id), id));
		t.insert(id);
	}	
	void remove_child(int id) { // Удаление дочернего узла
		if (!t.find(id)) {
			throw runtime_error("Error:" + to_string(id) + ":Node with that number doesn't exist.");
		}
		if (!check(id)) {
			throw runtime_error("Error:" + to_string(id) + ":Node is unavailable.");
		}
		send(Message(CommandType::REMOVE_CHILD, id, 0));
	}
	void start_heartbit() { // Начать проверку работоспособности всех узлов 
		if (!is_heartbit) {
			int time;
			cin >> time;
			heartbit_time = 4 * time;
			is_heartbit = true;
			if (pthread_create(&heartbits_thread, 0, heartbits_func, this) != 0) {
				throw runtime_error("Can not run second thread.");
			}
		}
		else {
			is_heartbit = false;
			if (pthread_join(heartbits_thread, NULL) != 0) {
				throw runtime_error("Error: can't join thread.");
			}
		}
	}
	void exec_child(int id) { // Выполнение команды на дочернем узле с индексом id
		int n;
		cin >> n;
		vector<int> v(n);
		for (int i = 0; i < n; i++) {
			cin >> v[i];
		}
		if (!t.find(id)) {
			throw runtime_error("Error:" + to_string(id) + ":Node with that number doesn't exist.");
		}
		if (!check(id)) {
			throw runtime_error("Error:" + to_string(id) + ":Node is unavailable.");
		}
		send(Message(CommandType::EXEC_CHILD, id, n, v.data(), 0));
	}
	pid_t get_pid() { // Возвращает pid
		return pid;
	}
	bool check(int id) { // Проверяет доступность узла
		Message msg(CommandType::RETURN, id, 0);
		send(msg);
		sleep(msg_wait_time);
		msg.get_to_id() = SERVER_ID;
		return last_msg == msg;
	}
	bool check(int id, int time) { // Проверяет доступность узла, ожидание в ms
		Message msg(CommandType::RETURN, id, 0);
		send(msg);
		usleep(4 * time);
		msg.get_to_id() = SERVER_ID;
		return last_msg == msg;
	}
	Socket*& get_publisher() {
		return publisher;
	}
	Socket*& get_subscriber() {
		return subscriber;
	}
	void* get_context() {
		return context;
	}
	tree& get_tree() {
		return t;
	}
	Message last_msg; // Сохраняет последнее сообщение
};

void* heartbits_func(void* server) { // Проверяет работоспособность всех узлов, пока команда не будет введена повторно
	Server* server_ptr = (Server*) server;
	while (server_ptr->is_heartbit) {
		usleep(server_ptr->heartbit_time/4);
		vector<int> tmp = server_ptr->get_tree().get_all_elems();
		bool not_answer = false;
		for (int& i : tmp) {
			if (!(server_ptr->check(i, server_ptr->heartbit_time))) {
				not_answer = true;
				cout << "Heartbit: node " << i <<  " is unavailable now\n";
			}
		}
		if (!not_answer) {
			cout << "OK\n";
		}
	}
}

void* subscriber_thread(void* server) { // Поток для получения сообщения
	Server* server_ptr = (Server*) server;
	pid_t serv_pid = server_ptr->get_pid();
	try {
		pid_t child_pid = fork();
		if (child_pid == -1) {
			throw runtime_error("Can not fork");
		}
		if (child_pid == 0) {
			execl("client", "client", "0", server_ptr->get_publisher()->get_endpoint().data(), "-1", nullptr);
			throw runtime_error("Can not execl");
			server_ptr->~Server();
			return (void*)-1;
		}
		string endpoint = create_endpoint(EndpointType::PARENT_PUB, child_pid);
		server_ptr->get_subscriber() = new Socket(server_ptr->get_context(), SocketType::SUBSCRIBER, endpoint);
		server_ptr->get_tree().insert(0);
		for (;;) {
			Message msg = server_ptr->get_subscriber()->receive();
			if (msg.command == CommandType::ERROR){
				throw invalid_argument("Wrong command");
			}
			server_ptr->last_msg = msg;
			if (msg.command == CommandType::CREATE_CHILD){
				cout << "OK:" << msg.get_create_id() << "\n";
			}
			else if (msg.command == CommandType::REMOVE_CHILD) {
				cout << "OK" << "\n";
				server_ptr->get_tree().delete_el(msg.get_create_id());
			}
			else if (msg.command == CommandType::EXEC_CHILD) {
				cout << "OK:" << msg.get_create_id() << ":" << msg.sum << "\n";
			}
		}
	} 
	catch (runtime_error& err) {
		cout << "Server wasn't started " << err.what() << "\n";
	} 
	catch (invalid_argument& err) {
		cout << err.what() << "\n";
	}
	return nullptr;
}

void process_cmd(Server& server, string cmd){ // Исполнение пользовательской команды
	if (cmd == "create") { // Создание узла
		int id;
		cin >> id;
		server.create_child(id);
	} 
	else if (cmd == "remove") { // Удаление узла
		int id;
		cin >> id;
		if (id == 0) {
			throw runtime_error("Can't remove root");
		}
		server.remove_child(id);
	} 
	else if (cmd == "exec") { // Выполнение задачи на узле
		int id;
		cin >> id;
		server.exec_child(id);
	} 
	else if (cmd == "exit") { // Выход из программы
		throw invalid_argument("Exiting...");
	} 
	else if (cmd == "heartbit") { // Проверка на работоспособность узлов
		server.start_heartbit();
	} 
	else if (cmd == "status") { // Проверка узла
		int id;
		cin >> id;
		if (!server.get_tree().find(id)) {
			throw runtime_error("Error:" + to_string(id) + ":Node with that number doesn't exist.");
		}
		if (server.check(id)) {
			cout << "OK" << "\n";
		} 
		else {
			cout << "Node is unavailable" << "\n";
		}
	}
	else {
		cout << "It is not a command!\n";
	}
}

Server* server_ptr = nullptr;
void TerminateByUser(int) { 
	if (server_ptr != nullptr) {
		server_ptr->~Server();
	}
	cout << to_string(getpid()) + " Terminated by user" << "\n";
	exit(0);
}

int main (int argc, char const *argv[]) {
	try{
		if (signal(SIGINT, TerminateByUser) == SIG_ERR) { // Обработка сигналов
			throw runtime_error("Can not set SIGINT signal");
		}
		if (signal(SIGSEGV, TerminateByUser) == SIG_ERR) { // Обработка сигналов
			throw runtime_error("Can not set SIGSEGV signal");
		}
		if (signal(SIGTERM, TerminateByUser) == SIG_ERR) { // Обработка сигналов
			throw runtime_error("Can not set SIGTERM signal");
		}
		Server server;
		server_ptr = &server;
		cout << getpid() << " server started correctly!\n";
		for (;;) {
			try {
				string cmd;
				while (cin >> cmd) {
					process_cmd(server, cmd);
				}
			} 
			catch (const runtime_error& arg) {
				cout << arg.what() << "\n";
			}
		}
	} 
	catch (const runtime_error& arg) {
		cout << arg.what() << endl;
	} 
	catch(...) {}
	sleep(7);
	return 0;
}
