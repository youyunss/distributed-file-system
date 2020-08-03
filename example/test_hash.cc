#include "consistent_hash.h"
#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include <iostream>
#include <sstream>
using namespace std;

int main() {
    char filemd5[36] = "da9341bb2db6212d17a55e220f015212";
    filemd5[32] = '\0';

    //ConsistentHash的使用方法
    int node_num = 3; 
    ConsistentHash* consistent_hash = new ConsistentHash(node_num);
    ssize_t server_index = consistent_hash->GetServerIndex(string(filemd5));

    //打印查看
    cout<<filemd5<<endl;
    cout<<server_index<<endl;

    return 0;
}


// // 一致性哈希分布性能查看
// int main() {
//     int node_num = 5;
//     int data_count = 100;
//     auto consistent_hash = new ConsistentHash(node_num);
//     vector<int> result(node_num, 0);   // 节点存放数据数目统计
//     srand(time(nullptr));
//     for (int i = 0; i < data_count; ++i) {
//         int value = rand();
//         stringstream ss;
//         ss << value;
//         string key = ss.str();
//         size_t index = consistent_hash->GetServerIndex(key);
//         result[index]++;
//         cout << i << ":" << "key = " << key << " index = " << index << endl;
//     }
//     cout<<endl;
//     for(int i=0;i<node_num;i++) {
//         cout << i << ":" << result[i] << endl;
//     }
// }
