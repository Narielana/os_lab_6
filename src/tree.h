#ifndef _TREE_H
#define _TREE_H

#include <vector>
using namespace std;

class tree_el { // Узел дерева
public:
	tree_el* left;
	tree_el* right;
	int value;
	tree_el(int new_val);
	int& get_value();
	tree_el*& get_left();
	tree_el*& get_right();
	friend bool operator<(tree_el& lhs, tree_el& rhs);
	friend bool operator==(tree_el& lhs, tree_el& rhs);
};

class tree {
public:
	tree_el* root; // Корень дерева
	void delete_tree(tree_el*& cur); // Удаление дерева
	static void print_tree(tree_el*& cur, int h = 0); // Распечатка дерева
	bool find_el(tree_el* cur, int val); // Поиск элемента
	void insert_el(tree_el*& cur, int new_val); // Добавление элемента
	void delete_el_tree(tree_el*& cur, int val); // Удалить элемент
	int get_parent(tree_el*& cur, int val); // Возвращает значение элемента родителя
	void get_all(tree_el* cur, vector<int>& tmp); // Передаёт все элементы в список
	tree();
	tree(int new_val);
	void insert(int new_val); // Добавить элемент в дерево
	bool find(int val); // Проверка, есть ли такой элемент в дереве
	void delete_el(int val); // Удалить элемент 
	int get_place(int val); // Возвращает значение элемента родителя
	vector<int> get_all_elems(); // Возвращает список со всеми элементами
	~tree();
};

#endif
