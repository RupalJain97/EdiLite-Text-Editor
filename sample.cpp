#include <iostream>

int main(){
	for(int i=30;i<=37;i++){
		std::cout <<"\033[" << i << "m " << i<< ": sample  \n";
	}
	std::cout<<"\n";
	for(int i=90;i<=97;i++){
		std::cout<<"\033[" << i << "m " << i<< ": sample \n";
	}
	std::cout<<"\n";
	return 0;
}
