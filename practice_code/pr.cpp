#include <iostream>
#include <string>

using namespace std::literals;

class Duck {
public:
    void SetName(const std::string& name) {
        name_ = name;
    }

    std::string GetName() {
        return name_;
    }

    int GetDistance() {
        return distance_;
    }

    void SetDistance(int distance) {
        distance_ = distance;
    }

    int GetTotalDistance() {
        return total_distance_;
    }

    void SetTotalDistance(int total_distance) {
        total_distance_ = total_distance;
    }

    void Fly(int distance) {
        SetDistance(distance);
        SetTotalDistance(GetTotalDistance() + distance);
        PrintData();
    }
    void PrintData() {
        std::cout << GetName() << " flies " << GetDistance() << "km. " <<
            "Total flight distance is " << GetTotalDistance() << "km." << '\n';
    }

private:
    std::string name_;
    int distance_ = 0;
    int total_distance_ = 0;
};

int main() {
    Duck duck1;
    duck1.SetName("Whisper Quack"s);
    Duck duck2;
    duck2.SetName("Fire Wing"s);

    int num_commands = 0;
    std::cin >> num_commands;

    for (int i = 0; i < num_commands; ++i) {
        int duck_number, distance;
        std::cin >> duck_number >> distance;
        if (duck_number == 1) {
            duck1.Fly(distance);
        } else if (duck_number == 2) {
            duck2.Fly(distance);
        }
    }
}
