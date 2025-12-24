#include "core/Common.hpp"
#include "core/MazeBuilder.hpp"
#include "core/MazeViewer.hpp"

int main() {
    
    while(1) {
        // Main loop code here
        int32_t seed;
        MazeType type; // Example maze type
        std::string input;
        std::cout << "press b to build a maze" << std::endl;
        std::cout << "press q to quit" << std::endl;
        std::cin >> input;
        if (input == "b") {
            std::cout << "Enter seed value: ";
            std::cin >> seed;
            std::cout << "Select maze type (Small, Medium, Large): ";
            std::string typeInput;
            std::cin >> typeInput;
            if (typeInput == "Small") {
                type = MazeType::Small;
            } else if (typeInput == "Medium") {
                type = MazeType::Medium;
            } else if (typeInput == "Large") {
                type = MazeType::Large;
            } else {
                std::cout << "Invalid maze type selected." << std::endl;
                continue;  
            } 
            Maze maze = MazeBuilder::Build(type, seed);
            MazeViewer viewer = MazeViewer().getMazeViewer(maze);
            viewer.displayMaze();
        } else if (input == "q") {
            break;
        } else {
            std::cout << "Invalid input. Please try again." << std::endl; 
        }
    }

    return 0;
}