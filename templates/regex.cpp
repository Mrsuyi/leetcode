#include <iostream>
#include <memory>
#include <unordered_set>
#include <vector>

using namespace std;

#pragma mark - Node

const char* node_type_2_str[] = {
    "CAT", "OR", "STAR", "CHAR", "END",
};

struct Node {
  enum Type {
    CAT = 0,
    OR,
    STAR,
    CHAR,
    END,
  };

  Node(Type type, char c = '\0', int idx = -1)
      : type(type), left(nullptr), right(nullptr), chr(c), idx(idx) {}

  // Calculate firstpos, lastpos, followpos.
  void calc();
  bool nullable() const;

  // Only for test purpose:
  // Recover regex pattern.
  string print_pattern() const;
  // Print tree.
  void print_tree(int depth = 0) const;
  // Print NFA.
  void print_nfa() const;

  Type type;
  Node* left;
  Node* right;
  char chr;
  int idx;
  unordered_set<Node*> firstpos;
  unordered_set<Node*> lastpos;
  unordered_set<Node*> followpos;
};

bool Node::nullable() const {
  switch (type) {
    case STAR:
      return true;
    case OR:
      return left->nullable() || right->nullable();
    case CAT:
      return left->nullable() && right->nullable();
    case CHAR:
    case END:
      return false;
  }
  assert(false);
  return true;
}

void Node::calc() {
  if (left)
    left->calc();
  if (right)
    right->calc();

  switch (type) {
    case CHAR:
    case END: {
      firstpos.insert(this);
      lastpos.insert(this);
      break;
    }
    case STAR: {
      firstpos = left->firstpos;
      lastpos = left->lastpos;
      // followpos.
      for (Node* last : lastpos) {
        for (Node* first : firstpos) {
          last->followpos.insert(first);
        }
      }
      break;
    }
    case CAT: {
      // firstpos.
      for (Node* first : left->firstpos) {
        firstpos.insert(first);
      }
      if (left->nullable()) {
        for (Node* first : right->firstpos) {
          firstpos.insert(first);
        }
      }
      // lastpos.
      for (Node* last : right->lastpos) {
        lastpos.insert(last);
      }
      if (right->nullable()) {
        for (Node* last : left->lastpos) {
          lastpos.insert(last);
        }
      }
      // followpos.
      for (Node* r : right->firstpos) {
        for (Node* l : left->lastpos) {
          l->followpos.insert(r);
        }
      }
      break;
    }
    case OR: {
      // firstpos.
      for (Node* first : left->firstpos) {
        firstpos.insert(first);
      }
      for (Node* first : right->firstpos) {
        firstpos.insert(first);
      }
      // lastpos.
      for (Node* last : left->lastpos) {
        lastpos.insert(last);
      }
      for (Node* last : right->lastpos) {
        lastpos.insert(last);
      }
      break;
    }
  }
}

std::string Node::print_pattern() const {
  switch (type) {
    case CAT:
      assert(left);
      assert(right);
      return left->print_pattern() + right->print_pattern();
    case OR:
      assert(left);
      assert(right);
      return left->print_pattern() + '|' + right->print_pattern();
    case STAR: {
      assert(left);
      assert(!right);
      string sub = left->print_pattern();
      if (sub.size() > 1) {
        return "(" + sub + ")*";
      } else {
        return sub + "*";
      };
    }
    case CHAR:
      return string(1, chr);
    case END:
      return "";
  }
  assert(false);
  return "";
}

void Node::print_tree(int depth) const {
  printf("%s", string(depth, '\t').c_str());
  printf("%llu %s %d '%c'\n", (unsigned long long)this, node_type_2_str[type],
         idx, chr);
  if (left)
    left->print_tree(depth + 1);
  if (right)
    right->print_tree(depth + 1);
}

void Node::print_nfa() const {
  if (left) {
    left->print_nfa();
  }
  switch (type) {
    case CHAR:
    case END:
      printf("Node %d '%c':\n\tfirstpos:\t", idx, chr);
      for (Node* first : firstpos)
        printf("%d ", first->idx);
      printf("\n\tlastpos:\t");
      for (Node* last : lastpos)
        printf("%d ", last->idx);
      printf("\n\tfollowpos:\t");
      for (Node* follow : followpos)
        printf("%d ", follow->idx);
      printf("\n");
      break;
    case CAT:
    case OR:
    case STAR:
      break;
  }
  if (right) {
    right->print_nfa();
  }
}

#pragma mark - Tree

struct Tree {
  Tree(const string& pattern);
  ~Tree();

  // Eat as much as possible until ')' or end.
  Node* eat();
  // Try to eat a '*'.
  Node* eat_star(Node* cur);

  Node* root;
  vector<Node*> nodes;
  const string pattern;
  size_t idx;
};

Tree::Tree(const string& pattern) : root(nullptr), pattern(pattern), idx(0) {
  root = eat();

  // Add trailing END node.
  Node* end = new Node(Node::END, '#');
  nodes.push_back(end);
  Node* cat = new Node(Node::CAT);
  nodes.push_back(cat);
  cat->left = root;
  cat->right = end;
  root = cat;

  root->calc();
}

Tree::~Tree() {
  for (size_t i = 0; i < nodes.size(); ++i)
    delete nodes[i];
}

Node* Tree::eat() {
  // printf("eat\n");
  Node* pre = nullptr;
  while (idx < pattern.size()) {
    // printf("%zu\n", idx);
    if (pattern[idx] == ')') {
      // printf("exit\n");
      return pre;
    }

    Node* cur = nullptr;
    // pre|cur.
    if (pattern[idx] == '|') {
      ++idx;
      // "null|".
      Node* nxt = eat();
      if (pre) {
        cur = new Node(Node::OR);
        nodes.push_back(cur);
        cur->left = pre;
        cur->right = nxt;
        pre = cur;
      } else {
        pre = nxt;
      }
    }
    // pre&cur.
    else {
      // ().
      if (pattern[idx] == '(') {
        ++idx;
        cur = eat();
        assert(pattern[idx] == ')');
        ++idx;
      }
      // normal char.
      else {
        cur = new Node(Node::CHAR, pattern[idx], idx);
        ++idx;
      }
      cur = eat_star(cur);

      if (pre) {
        Node* cat = new Node(Node::CAT);
        nodes.push_back(cat);
        cat->left = pre;
        cat->right = cur;
        pre = cat;
      } else {
        pre = cur;
      }
    }
  }
  return pre;
}

Node* Tree::eat_star(Node* cur) {
  if (idx == pattern.size() || pattern[idx] != '*') {
    return cur;
  }
  ++idx;
  // Ignore "()*".
  if (!cur)
    return nullptr;
  Node* star = new Node(Node::STAR);
  nodes.push_back(star);
  star->left = cur;
  return star;
}

#pragma mark - Regex

struct Regex {
  Regex(const string& pattern);
};

int main() {
  // printf("a|b\n");
  Tree t1("a|b");
  assert(t1.root->print_pattern() == "a|b");
  // t1.root->print_tree();
  // t1.root->print_nfa();

  printf("(a|b)*abb\n");
  Tree t2("(a|b)*abb");
  assert(t2.root->print_pattern() == "(a|b)*abb");
  // t2.root->print_tree();
  t2.root->print_nfa();

  return 0;
}
