#include "rclcpp/rclcpp.hpp"
class MyNode : public rclcpp::Node
{
public:
MyNode():Node("cpp_test")
{
   
}
private:
};

int main(int argc,char **argv)
{
    rclcpp::init(argc,argv);//
auto node=std::make_shared<MyNode>();//create a node
rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}
