#include <iostream>
#include <uwebsockets/App.h>
#include <vector>
#include <charconv>
#include <algorithm>
#include <regex>
#include <map>
#include <string>

using namespace std;

map<string, string> bot_commands = {
    {"hello" , "Hi!"},
    {"whats up" , "Answering questions."},
    {"what is your name" , "You can call me AnswerGenerator322."},
    {"how are you" , "Not bad for the program."},
    {"online", "Users online:"}
};

string getAnswer(string& user_question)
{
    transform(user_question.begin(), user_question.end(), user_question.begin(), ::tolower);
    for (const auto& [question, answer] : bot_commands) {
        regex expression = regex(".*" + question + ".*");
        if (regex_match(user_question, expression)) {
            return "[BOT]: " + answer;
        }
    }
    return "[BOT]: I don't know what to say.";
}

const string set_name = "SET_NAME::";
const string direct = "DIRECT::";
const string subscribe = "SUBSCRIBE::";

bool isSetNameCommand(const string_view& message) {
    return message.find(set_name) == 0;
}

bool isDirectCommand(const string_view& message) {
    return message.find(direct) == 0;
}

bool isSubscribeCommand(const string_view& message) {
    return message.find(subscribe) == 0;
}

string parseName(const string_view& message) {
    string user_name = string(message.substr(set_name.length()));
    return user_name;
}

string parseRecieverId(const string_view& message) {
    string_view rest = message.substr(direct.length());
    int pos = rest.find("::");
    string_view id = rest.substr(0, pos);
    return string(id);
}

string parseDirectMessage(const string_view& message) {
    string_view rest = message.substr(direct.length());
    int pos = rest.find("::");
    string_view text = rest.substr(pos + 2);
    return string(text);
}

string parseSubscribe(const string_view& message) {
    string channel = string(message.substr(subscribe.length()));
    return channel;
}

bool isNameValid(const string& name) {
    if (name.find("::") == string::npos && name.length() <= 255) {
        return true;
    }
    return false;
}

bool isUserWithIdExist(const int& id, const int& last_user_id, const vector<int>& free_id) {
    if (id >= last_user_id || id <= 0) {
        return false;
    }
    for (const auto& it : free_id) {
        if (id == it) {
            return false;
        }
    }
    return true;
}

int main() {
    struct PerSocketData {
        int user_id;
        string name;
    };

    int last_user_id = 2;
    int users_count = 0;
    map<int, string> users_online;
    vector<int> free_id;
    vector<string> group_chat;

    uWS::App()
        .ws<PerSocketData>("/*",
            {
                .compression = uWS::SHARED_COMPRESSOR,
                .maxPayloadLength = 16 * 1024,
                .idleTimeout = 600,
                .maxBackpressure = 1 * 1024 * 1024,
                .upgrade = nullptr,

                .open = [&last_user_id, &users_count, &free_id](auto* connection) {
                    cout << "New connection created" << endl;
                    users_count++;
                    cout << "Total users connected: " << users_count << endl;
                    PerSocketData* userData = (PerSocketData*)connection->getUserData();
                    if (free_id.size() != 0) {
                        userData->user_id = free_id[free_id.size() - 1];
                        free_id.pop_back();
                    }
                    else {
                        userData->user_id = last_user_id++;
                    }
                    userData->name = "UNNAMED";
                    connection->subscribe("broadcast");
                    connection->subscribe("user#" + to_string(userData->user_id));
                },
                .message = [&free_id, &last_user_id, &users_online, &group_chat](auto* connection, string_view message, uWS::OpCode opCode) {
                    cout << "New message recieved: " << message << endl;
                    PerSocketData* userData = (PerSocketData*)connection->getUserData();

                    if (isSetNameCommand(message)) {
                        cout << "User set their name" << endl;
                        string user_name = parseName(message);
                        if (isNameValid(user_name)) {
                            userData->name = parseName(message);
                            users_online[userData->user_id] = userData->name;
                            for (const auto& [id, name] : users_online) {
                                connection->send(name + '#' + to_string(id) + " Online", opCode, true);
                            }
                            connection->publish("broadcast", userData->name + '#' + to_string(userData->user_id) + " Online");
                        }
                        else {
                            connection->send("Invalid name ", opCode, true);
                        }
                    }

                    if (isDirectCommand(message)) {
                        cout << "User sent direct message" << endl;
                        string id = parseRecieverId(message);
                        string text = parseDirectMessage(message);
                        bool flag_id = true;
                        for (const auto& it : group_chat) {
                            if (id == it) {
                                flag_id = false;
                            }
                        }
                        if (flag_id) {
                            int id_i = stoi(id);

                            if (id_i == 1) {
                                string answer = getAnswer(text);
                                connection->send(answer, opCode, true);
                                if (answer == "[BOT]: Users online:") {
                                    for (const auto& [id, name] : users_online) {
                                        connection->send(name + '#' + to_string(id), opCode, true);
                                    }
                                }
                            }
                            if (!isUserWithIdExist(id_i, last_user_id, free_id)) {
                                connection->send("Error, there is no user with ID = " + id, opCode, true);
                            }
                            else {
                                connection->publish("user#" + id, userData->name + '#' + to_string(userData->user_id) + ": " + text);
                            }
                        }
                        else {
                            connection->publish(id, userData->name + '#' + to_string(userData->user_id) + ": " + text);
                        }
                    }

                    if (isSubscribeCommand(message)) {
                        string channel = parseSubscribe(message);
                        cout << "User subscribed to channel \"" << channel << "\"" << endl;;
                        group_chat.push_back(channel);
                        connection->subscribe(channel);
                    }

                },
                .close = [&users_count, &free_id, &users_online](auto* connection, int /*code*/, std::string_view /*message*/) {
                    cout << "Connection closed" << endl;
                    users_count--;
                    PerSocketData* userData = (PerSocketData*)connection->getUserData();
                    free_id.push_back(userData->user_id);
                    connection->publish("broadcast", userData->name + '#' + to_string(userData->user_id) + " Offline");
                    users_online.erase(userData->user_id);
                    cout << "Total users connected: " << users_count << endl;
                }
            }
            ).listen(9001, [](auto* listen_socket) {
                    if (listen_socket) {
                        cout << "Listening on port " << 9001 << endl;
                    }
                }).run();
}