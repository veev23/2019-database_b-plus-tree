#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <deque>
using namespace std;
class LeafNode;
class NonLeafNode;
class IndexEntry {
public:
	IndexEntry(int key, int nextlevelBID) {
		this->Key = key;
		this->NextLevelBID = nextlevelBID;
	}
	int Key;
	int NextLevelBID;
};
class DataEntry {
public:
	DataEntry() {}
	DataEntry(int key, int value) {
		this->Key = key;
		this->Value = value;
	}
	int Key;
	int Value;
};
class BTree {
private:
	int rootBID;
	int depth;
	//BTree가 가지고 있는 block의 수
	int num_of_blocks;
	const char* file_name;
	int block_size;
	//블럭 사이즈에 따른 Node가 가지고 있을 최대 entry 수
	int max_entry_num;
	class NonLeafNode {
	public:
		NonLeafNode(int entry_num) {
			for (int i = 0; i < entry_num; i++) {
				entry.push_back(IndexEntry(0, 0));
			}
		}
		int next_level_BID;
		//entry.nextLevelBID[i]는 entry.Key[i]보다 작은 블럭을 가리킴
		vector<IndexEntry> entry;
	};
	class LeafNode {
	public:
		vector<DataEntry> entry;
		int next_block_BID;
	};
public:
	BTree(int root, int depth, const char* file_name, int block_size); 
	bool insert_from_nonleaf(int key, deque<int>& BIDs, int left_child, int right_child);
	bool insert_from_leaf(int key, int rid);
	void print(const char* output_path);
	deque<int> find_index_blocks_by_key(int key);
	int search(int key, const char* output_path); // point search
	int search(int startRange, int endRange, const char* output_path); // range search

	NonLeafNode* get_block(int BID) {
		ifstream file(file_name, ios::binary);
		file.seekg(12 + (BID - 1)*block_size);
		NonLeafNode* node = new NonLeafNode(max_entry_num);
		int nBID, key;
		for (int i = 0; i < max_entry_num; i++) {
			file.read((char*)&nBID, sizeof(int));
			file.read((char*)&key, sizeof(int));
			node->entry.push_back(IndexEntry{ nBID, key });
		}
		file.read((char*)&node->next_level_BID, sizeof(int));
		file.close();
		return node;
	}
	LeafNode* get_block(int BID, int depth) {
		ifstream file(file_name, ios::binary);
		file.seekg(12 + (BID - 1)*block_size);
		LeafNode* node = new LeafNode();
		int key, value;
		for (int i = 0; i < max_entry_num; i++) {
			file.read((char*)&key, sizeof(int));
			file.read((char*)&value, sizeof(int));
			if (key == NULL) break;
			node->entry.push_back(DataEntry{ key, value });
		}
		file.read((char*)&node->next_block_BID, sizeof(int));
		file.close();
		return node;
	}
	void write_block(int BID, LeafNode* node) {
		ofstream file(file_name, ios::binary | ios::in | ios::out);
		file.seekp(12 + (BID - 1)*block_size);
		for (int i = 0; i < node->entry.size(); i++) {
			file.write((char*)&node->entry[i].Key, sizeof(int));
			file.write((char*)&node->entry[i].Value, sizeof(int));
		}
		int zero = 0;
		for (int i = node->entry.size(); i < max_entry_num; i++) {
			file.write((char*)&zero, sizeof(int));
			file.write((char*)&zero, sizeof(int));
		}
		file.write((char*)&node->next_block_BID, sizeof(int));
		file.close();
	}
	void write_block(int BID, NonLeafNode* node) {
		ofstream file(file_name, ios::binary | ios::in | ios::out);
		file.seekp(12 + (BID - 1)*block_size);
		for (int i = 0; i < max_entry_num; i++) {
			file.write((char*)&node->entry[i].NextLevelBID, sizeof(int));
			file.write((char*)&node->entry[i].Key, sizeof(int));
		}
		file.write((char*)&node->next_level_BID, sizeof(int));
		file.close();
	}
	const int& get_rootBID() {
		return rootBID;
	}
	void set_rootBID(int bid) {
		rootBID = bid;
	}
	const int& get_depth() {
		return depth;
	}
	void set_depth(int depth) {
		this->depth = depth;
	}
};
BTree::BTree(int root, int depth, const char* file_name, int block_size) {
	this->rootBID = root;
	this->depth = depth;
	this->file_name = file_name;
	this->block_size = block_size;
	this->max_entry_num = (block_size - 4) / 8;
}
template<typename T>
vector<T> vector_slice(vector<T> const& v, int m, int n) {
	vector<T> vec(n-m+1);
	copy(v.begin() + m, v.begin() + n + 1, vec.begin());
	return vec;
}
bool BTree::insert_from_nonleaf(int key, deque<int>& BIDs, int left_child, int right_child) {
	if (BIDs.empty()) {
		rootBID = ++num_of_blocks;
		depth++;
		NonLeafNode* nonleaf_node = new NonLeafNode(max_entry_num);
		nonleaf_node->entry[0]=IndexEntry(key, left_child);
		nonleaf_node->entry[1].NextLevelBID = right_child;
		write_block(rootBID, nonleaf_node);
	}
	return true;
}
bool BTree::insert_from_leaf(int key, int rid) {
	//루트가 없을 때
	if (this->rootBID == 0) {
		this->rootBID = 1;
		LeafNode node;
		node.entry.push_back(DataEntry(key, rid));
		num_of_blocks++;
		depth++;
		write_block(1, &node);
		return true;
	}
	deque<int> block_num = find_index_blocks_by_key(key);
	LeafNode* insert_node;
	int insert_node_block_num = block_num.back();
	block_num.pop_back();
	insert_node = get_block(insert_node_block_num, depth);

	insert_node->entry.push_back(DataEntry{ key, rid });
	for (int i = insert_node->entry.size() - 1; i > 0; i--) {
		if (insert_node->entry[i].Key < insert_node->entry[i - 1].Key) {
			swap(insert_node->entry[i], insert_node->entry[i - 1]);
		}
		else break;
	}
	//split해야할 때(entry의 size가 block size를 넘는 경우)
	if (insert_node->entry.size() > max_entry_num) {
		int right_node_block_num = ++num_of_blocks;
		//새로 split될 노드
		LeafNode* right_node = new LeafNode();

		right_node->next_block_BID = insert_node->next_block_BID;
		insert_node->next_block_BID = right_node_block_num;

		//spilt
		right_node->entry = vector_slice(insert_node->entry, (max_entry_num-1) / 2+1, max_entry_num);
		insert_node->entry = vector_slice(insert_node->entry, 0,(max_entry_num-1) / 2);

		insert_from_nonleaf(right_node->entry[0].Key, block_num, insert_node_block_num, right_node_block_num);
		write_block(right_node_block_num, right_node);
	}
	write_block(insert_node_block_num, insert_node);
	return true;
}
void BTree::print(const char* output_path) {
	ifstream file(file_name, ios::binary);
	ofstream output(output_path, ios::in | ios::out);
	if (depth == 0) {
		//아무것도 없음
	}
	else if (depth == 1) {
		LeafNode* tmp = get_block(rootBID, depth);
		output << "<0>\n";
		for (int i = 0; i < tmp->entry.size(); i++) {
			if (tmp->entry[i].Key == NULL) break;
			output << tmp->entry[i].Key;
			if (tmp->entry.size() - 1 > i) output << ', ';
			else output << '\n';
		}
	}
	else if (depth == 2) {
		NonLeafNode* tmp = get_block(rootBID);
		output << "<0>\n";
		for (int i = 0; i < tmp->entry.size(); i++) {
			if (tmp->entry[i].Key == NULL) break;
			output << tmp->entry[i].Key;
			if (tmp->entry.size() - 1 > i) output << ', ';
			else output << '\n';
		}
		output << "<1>\n";
		LeafNode* child = get_block(tmp->entry[0].NextLevelBID, depth);
		for (int i = 0; i < child->entry.size(); i++) {
			output << child->entry[i].Key;
			if (child->entry.size() - 1 > i) {
				output << ', ';
			}
			if (child->entry.size() - 1 == i) {
				i = 0;
				if (child->next_block_BID == NULL) break;
				child = get_block(child->next_block_BID, depth);
			}
		}
	}
	else {
		NonLeafNode* tmp = get_block(rootBID);
		output << "<0>\n";
		for (int i = 0; i < tmp->entry.size(); i++) {
			if (tmp->entry[i].Key == NULL) break;
			output << tmp->entry[i].Key;
			if (tmp->entry.size() - 1 > i) output << ', ';
			else output << '\n';
		}
		output << "<1>\n";
		for (int sec = 0; sec < tmp->entry.size(); sec++) {
			NonLeafNode* child = get_block(tmp->entry[sec].NextLevelBID);
			for (int i = 0; i < child->entry.size(); i++) {
				if (child->entry[i].Key == NULL) break;
				output << child->entry[i].Key;
				if (tmp->entry.size() - 1 > sec || child->entry.size() - 1 > i) {
					output << ', ';
				}
			}
		}
	}
	output.close();
	file.close();
}
//순서대로 접근한 Block Number를 return
deque<int> BTree::find_index_blocks_by_key(int key) {
	deque<int> dq;
	if (depth == 0) {
		return dq;
	}
	else if (depth == 1) {
		dq.push_back(rootBID);
		return dq;
	}
	else {
		int now_depth = 1;
		NonLeafNode* tmp = get_block(rootBID);
		dq.push_back(rootBID);
		//NonLeaf->NonLeaf로 탐색
		while (now_depth < depth - 1) {
			bool find = false;
			for (auto iter : tmp->entry) {
				if (iter.Key > key) {
					dq.push_back(iter.NextLevelBID);
					tmp = get_block(iter.NextLevelBID);
					find = true;
					break;
				}
			}
			if (!find) {
				dq.push_back(tmp->next_level_BID);
				tmp = get_block(tmp->next_level_BID);
			}
			now_depth++;
		}
		//NonLeaf->Leaf로 탐색
		for (auto iter : tmp->entry) {
			if (iter.Key > key) {
				dq.push_back(iter.NextLevelBID);
				return dq;
			}
		}
		dq.push_back(tmp->next_level_BID);
		return dq;
	}
}
int BTree::search(int key, const char* output_path) {
	ifstream file(file_name, ios::binary);
	ofstream output(output_path, ios::app);
	deque<int> block_num = find_index_blocks_by_key(key);
	if (block_num.empty()) {
		output << "fail\n";
	}
	bool found = false;
	LeafNode* searchNode = get_block(block_num.back(), depth);
	//LeafNode에서 탐색
	if (searchNode) {
		for (auto iter : searchNode->entry) {
			if (iter.Key == key) {
				found = true;
				output << iter.Key << ',' << iter.Value << ' ';
			}
		}
	}
	if (!found) {
		output << "not found";
	}
	output << '\n';
	return 0;
}
int BTree::search(int startRange, int endRange, const char* output_path) {
	ifstream file(file_name, ios::binary);
	ofstream output(output_path, ios::app);
	deque<int> block_num = find_index_blocks_by_key(startRange);
	if (block_num.empty()) {
		output << "fail\n";
	}
	bool found = false;
	LeafNode* searchNode = get_block(block_num.back(), depth);
	//LeafNode에서 탐색
	for (int i = 0; i < searchNode->entry.size(); i++) {
		if (startRange <= searchNode->entry[i].Key &&searchNode->entry[i].Key < endRange) {
			found = true;
			output << searchNode->entry[i].Key << ',' << searchNode->entry[i].Value << ' ';
			if (searchNode->entry.size() - 1 > i) {
				output << ', ';
			}
			if (searchNode->entry.size() - 1 == i) {
				i = 0;
				if (searchNode->next_block_BID == NULL) break;
				searchNode = get_block(searchNode->next_block_BID, depth);
			}
		}
		else break;
	}
	if (!found) {
		output << "not found";
	}
	output << '\n';
	return 0;
}
int main(int argc, char* argv[])
{
	char command = 'i';
	const char* file_name = "btree.bin";
	int block_size = 36;
	const char* input_file_path = "insert.txt";
	const char* output_file_path = "output.txt";
	if (argc > 1) {
		command = argv[1][0];
	}
	if (argc > 2) {
		file_name = argv[2];
	}
	if (argc > 3) {
		block_size = atoi(argv[3]);
	}
	int root = 0, depth = 0;
	if (command != 'c') {
		ifstream database_file(file_name, ios::binary);
		database_file.seekg(0);
		database_file.read((char*)&block_size, sizeof(int));
		database_file.read((char*)&root, sizeof(int));
		database_file.read((char*)&depth, sizeof(int));
		database_file.close();
	}
	BTree* myBtree = new BTree(root, depth, file_name, block_size);
	switch (command)
	{
	case 'c': {
		// create index file
		//create header
		//block size, root block id, depth
		ofstream database_file_o(file_name, ios::binary);
		database_file_o.seekp(0);
		database_file_o.write((char*)&block_size, sizeof(block_size));
		database_file_o.write((char*)&myBtree->get_rootBID(), sizeof(int));
		database_file_o.write((char*)&myBtree->get_depth(), sizeof(int));		database_file_o.close();		cout << "Create File : " << file_name << "\n";
		break;
	}
	case 'i': {
		// insert records from [records data file], ex) records.txt
		if (argc > 2)input_file_path = argv[3];
		ifstream input_file(input_file_path);
		if (!input_file.is_open()) {
			cout << "파일이 열리지 않음\n";
			return -1;
		}
		string str;
		while (!input_file.eof()) {
			input_file >> str;
			stringstream sstream(str);
			vector<int> token;
			string tmp;
			while (getline(sstream, tmp, ',')) {
				token.push_back(stoi(tmp));
			}
			myBtree->insert_from_leaf(token[0], token[1]);
			token.clear();
		}
		input_file.close();		cout << "insert complete : " << file_name << "\n";
		break;
	}
	case 's': {
		// search keys in [input file] and print results to [output file]
		if (argc > 2)input_file_path = argv[3];
		if (argc > 3)output_file_path = argv[4];
		ifstream input_file(input_file_path);
		if (!input_file.is_open()) {
			cout << "파일이 열리지 않음\n";
			return -1;
		}
		string str;
		while (!input_file.eof()) {
			input_file >> str;
			myBtree->search(stoi(str), output_file_path);
		}
		input_file.close();		cout << "search complete : " << output_file_path << "\n";
		break;
	}
	case 'r': {
		// search keys in [input file] and print results to [output file]
		if (argc > 2)input_file_path = argv[3];
		if (argc > 3)output_file_path = argv[4];
		ifstream input_file(input_file_path);
		if (!input_file.is_open()) {
			cout << "파일이 열리지 않음\n";
			return -1;
		}
		string str;
		while (!input_file.eof()) {
			input_file >> str;
			stringstream sstream(str);
			vector<int> token;
			string tmp;
			while (getline(sstream, tmp, ',')) {
				token.push_back(stoi(tmp));
			}
			myBtree->search(token[0], token[1], output_file_path);
			token.clear();
		}
		input_file.close();		cout << "range search complete : " << output_file_path << "\n";
		break;
	}
	case 'p':
		// print B+-Tree structure to [output file]
		if (argc > 2)output_file_path = argv[3];
		myBtree->print(output_file_path);		cout << "print complete : " << output_file_path << "\n";
		break;
	}
	ofstream save_header(file_name, ios::binary | ios::in | ios::out);
	save_header.seekp(4);
	save_header.write((char*)&myBtree->get_rootBID(), sizeof(int));
	save_header.write((char*)&myBtree->get_depth(), sizeof(int));	save_header.close();	return 0;}