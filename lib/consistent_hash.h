#include <map>
#include <string>
#include <sstream>
#include <functional>
#include <string>
using namespace std;

#define NODE_NUM 2
class ConsistentHash {
public:
    ConsistentHash() {
        Init();
    }

    ConsistentHash(int node_num) : node_num_(node_num)
    {
        Init();
    }

    ~ConsistentHash() 
    {
        server_nodes_.clear();
    }

    void Init() 
    {
        for (int i=0; i<node_num_; i++) {
            for( int j=0; j<vnode_num_; j++) {
                stringstream node_key;
                node_key << "SHARD-" << i << "-NODE-" << j;
                size_t partition = hash<string>{}(node_key.str());
                server_nodes_.insert(pair<size_t, size_t>(partition, i));
            }
        }
    }


    size_t GetServerIndex(const string& key)
    {
        size_t partition = hash<string>{}(key);
        auto it = server_nodes_.lower_bound(partition);
        if (it == server_nodes_.end()) {
            return server_nodes_.begin()->second;
        }
        return it->second;
    }


    void DeleteNode(int index)
    {
        for (int j = 0; j < vnode_num_; ++j) {
            stringstream node_key;
            node_key << "SHARD-" << index << "-NODE-" << j;
            size_t partition = hash<string>{}(node_key.str());
            auto it = server_nodes_.find(partition);
            if (it != server_nodes_.end()) {
                server_nodes_.erase(it);
            }
        }
    }


    void AddNode(int index)
    {
        for (int j = 0; j < vnode_num_; ++j) {
            stringstream node_key;
            node_key << "SHARD-" << index << "-NODE-" << j;
            size_t partition = hash<string>{}(node_key.str());
            server_nodes_.insert(pair<size_t, size_t>(partition, index));
        }
    }

private:
    map<size_t, size_t> server_nodes_;
    int node_num_ = 3;
    int vnode_num_ = 100;
};