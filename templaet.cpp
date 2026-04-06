#include "rclcpp/rclcpp.hpp"
class MyNode : public rclcpp::Node
{
public:
MyNode():Node("cpp_test")
{
   123456
}
private:123456
lq lq lq lq
};

int main(int argc,char **argv)
{
    rclcpp::init(argc,argv);//
auto node=std::make_shared<MyNode>();//create a node
rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
