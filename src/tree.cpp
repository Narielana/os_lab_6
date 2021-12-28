#include <iostream>
#include "tree.h"
using namespace std;

tree_el::tree_el(int new_val) : value(new_val), left(nullptr), right(nullptr) {}

int& tree_el::get_value() {
	return value;
}

tree_el*& tree_el::get_left() {
	return left;
}

tree_el*& tree_el::get_right() {
	return right;
}

bool operator < (tree_el& lhs, tree_el& rhs) {
	return lhs.get_value() < rhs.get_value();
} 

bool operator == (tree_el& lhs, tree_el& rhs) {
	return lhs.get_value() == rhs.get_value();
} 

void tree::delete_tree(tree_el*& cur) { // Удалить дерево
	if(!cur) {
		return;
	}
	delete_tree(cur->get_left());
	delete_tree(cur->get_right());
	delete cur;
}

void tree::print_tree(tree_el*& cur, int h) { // Печать дерева
	if(!cur) {
		return;
	}
	print_tree(cur->get_left(), h + 1);
	for (int i = 0; i < h; i++) {
		cout << " ";
	}
	cout << cur->get_value() << endl;
	print_tree(cur->get_right(), h + 1);
}

void tree::get_all(tree_el* cur, vector<int>& tmp) { // Передаёт все элементы в список
	if(!cur) {
		return;
	}
	get_all(cur->get_left(), tmp);
	tmp.push_back(cur->get_value());
	get_all(cur->get_right(), tmp);
} 

vector<int> tree::get_all_elems() { // Передаёт все элементы в список
	vector<int> tmp;
	get_all(root, tmp);
	return tmp;

}

bool tree::find_el(tree_el* cur, int val) { // Найти элемент
	if (cur == nullptr) {
		return false;
	}
	if (val < cur->get_value()) {
		return find_el(cur->get_left(), val);
	} 
	else if (val > cur->get_value()) {
		return find_el(cur->get_right(), val);
	} 
	else {
		return true; 
	}
}

void tree::insert_el(tree_el*& cur, int new_val) { // Добавить элемент
	if(cur == nullptr) {
		cur = new tree_el(new_val);
	} 
	else if(new_val < cur->get_value()) {
		return insert_el(cur->get_left(), new_val);
	} 
	else if (new_val > cur->get_value()) {
		return insert_el(cur->get_right(), new_val);
	}
}


tree::tree():root(nullptr) {}

tree::tree(int new_val) {
	root = new tree_el(new_val);
}

void tree::insert(int new_val) { // Добавить элемент
	insert_el(root, new_val);
}

bool tree::find(int val) { // Поиск элемента
	return find_el(root, val);
}

void tree::delete_el_tree(tree_el*& cur, int val) { // Пробегает по дереву до нужного элемента
	if (!cur) {
		return;
	}
	if (val < cur->get_value()) {
		delete_el_tree(cur->get_left(), val);
	} 
	else if (val > cur->get_value()) {
		delete_el_tree(cur->get_right(), val);
	} 
	else {
		delete_tree(cur);
		cur = nullptr;
	}
}

void tree::delete_el(int val) {
	delete_el_tree(root, val);
}

int tree::get_parent(tree_el*& cur, int val) { // Возвращает значение элемента родителя
	if (val < cur->get_value()) {
		if (cur->get_left() == nullptr) {
			return cur->get_value();
		}
		return get_parent(cur->get_left(), val);
	} 
	else if (val > cur->get_value()) {
		if (cur->get_right() == nullptr) {
			return cur->get_value();
		}
		return get_parent(cur->get_right(), val);
	}
	return -1;
}

int tree::get_place(int val) { // Возвращает значение элемента родителя
	return get_parent(root, val);
}

tree::~tree() {
	delete_tree(root);
}
