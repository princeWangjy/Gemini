#include <fstream>
#include <iostream>
#include <vector>
#include<string>
#include <stdint.h>

using namespace std;

struct Empty {};

typedef uint32_t VertexId;
typedef uint64_t EdgeId;

template <typename EdgeData> struct EdgeUnit {
  VertexId src;
  VertexId dst;
  EdgeData edge_data;
} __attribute__((packed));

template <> struct EdgeUnit<Empty> {
  VertexId src;
  union {
    VertexId dst;
    Empty edge_data;
  };
} __attribute__((packed));

bool endsWith(const std::string &str, const std::string &suffix) {
  if (str.length() < suffix.length()) {
    return false;
  }
  return str.compare(str.length() - suffix.length(), suffix.length(), suffix) ==
         0;
}

void split(string s, string delimiter, vector<string> &res) {
  size_t pos_start = 0, pos_end, delim_len = delimiter.length();
  string token;
  res.clear();

  while ((pos_end = s.find(delimiter, pos_start)) != string::npos) {
    token = s.substr(pos_start, pos_end - pos_start);
    pos_start = pos_end + delim_len;
    res.push_back(token);
  }

  res.push_back(s.substr(pos_start));
}

int main(int argc, char **argv) {
  char *fin = argv[1];
  char *fout = argv[2];
  cout << "input: " << fin << endl;
  cout << "output: " << fout << endl;

  std::ifstream in(fin);
  std::ofstream out(fout, std::ios::binary);

  string line;
  vector<string> container;
  vector<EdgeUnit<Empty>> edges;
  EdgeUnit<Empty> edge;
  uint64_t count = 0;

  if (in.is_open()) {
    while (std::getline(in, line)) {
      
      if(endsWith(string(fin),".gr")){
        split(line, " ", container);
        if(container[0]=="a"){
          edge.src = (uint32_t)std::stoi(container[1]);
          edge.dst = (uint32_t)std::stoi(container[2]);
        }
      }else{
        split(line, "\t", container);
        edge.src = (uint32_t)std::stoi(container[0]);
        edge.dst = (uint32_t)std::stoi(container[1]);
      }

      edges.push_back(edge);
      if ((++count) % 1000000 == 0) {
        cout << "Process " << count / 1000000 << "M edges" << endl;
        for (EdgeUnit<Empty> e : edges) {
          out.write((char *)&e, sizeof(e));
        }
        edges.clear();
      }
    }
  } else {
    cerr << "input file not open" << endl;
  }

  for (EdgeUnit<Empty> e : edges) {
    out.write((char *)&e, sizeof(e));
  }

  return 0;
}
