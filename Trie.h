#ifndef TRIE_H
#define TRIE_H

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
using namespace std;

struct TrieNode {
    unordered_map<char, TrieNode*> children;
    vector<int> ids;   // places whose name (or a word in it) starts here and ends at this node
};

// Prefix search for place names: typing "ka" finds "Kalsang Corner".
// Every word of a name is inserted, so "deer" also finds "Malsi Deer park".
class Trie {
private:
    TrieNode* root;

    void freeNode(TrieNode* node) {
        for (auto& c : node->children)
            freeNode(c.second);
        delete node;
    }

    void collect(TrieNode* node, set<int>& result) {
        for (int i = 0; i < node->ids.size(); i++)
            result.insert(node->ids[i]);
        for (auto& c : node->children)
            collect(c.second, result);
    }

    static char lower(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
        return c;
    }

    void insertText(string text, int id) {
        TrieNode* node = root;
        for (int i = 0; i < text.size(); i++) {
            char c = lower(text[i]);
            if (node->children.count(c) == 0)
                node->children[c] = new TrieNode();
            node = node->children[c];
        }
        node->ids.push_back(id);
    }

public:
    Trie() {
        root = new TrieNode();
    }

    ~Trie() {
        freeNode(root);
    }

    void insert(string name, int id) {
        // "Malsi Deer park" -> "malsi deer park", "deer park", "park"
        for (int i = 0; i < name.size(); i++)
            if (i == 0 || name[i - 1] == ' ')
                insertText(name.substr(i), id);
    }

    // ids of all places with a word starting with prefix
    vector<int> startsWith(string prefix) {
        TrieNode* node = root;
        for (int i = 0; i < prefix.size(); i++) {
            char c = lower(prefix[i]);
            if (node->children.count(c) == 0) return vector<int>();
            node = node->children[c];
        }
        set<int> result;
        collect(node, result);
        return vector<int>(result.begin(), result.end());
    }
};

#endif
