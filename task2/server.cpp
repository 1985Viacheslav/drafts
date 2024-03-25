#include "json_type_handlers.h"

#include <iostream>
#include <string>
#include <vector>

// #include <Poco/Data/MySQL/Connector.h>
// #include <Poco/Data/MySQL/MySQLException.h>
#include <Poco/Data/PostgreSQL/Connector.h>
#include <Poco/Data/PostgreSQL/PostgreSQLException.h>
#include <Poco/Data/SessionFactory.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Array.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Dynamic/Var.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/MessageHeader.h>
#include <Poco/Timestamp.h>
#include <Poco/DateTimeFormatter.h>
#include <Poco/DateTimeFormat.h>
#include <Poco/Exception.h>
#include <Poco/ThreadPool.h>
#include <Poco/Util/ServerApplication.h>
#include <Poco/Util/Option.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/HelpFormatter.h>
#include <Poco/Crypto/DigestEngine.h>


using namespace Poco::Data::Keywords;
using Poco::Data::Session;
using Poco::Data::Statement;
using Poco::Net::HTTPRequestHandler;
using Poco::Net::HTTPRequestHandlerFactory;
using Poco::Net::HTTPServer;
using Poco::Net::HTTPServerParams;
using Poco::Net::HTTPServerRequest;
using Poco::Net::HTTPServerResponse;
using Poco::Net::MessageHeader;
using Poco::Net::ServerSocket;
using Poco::Util::Application;
using Poco::Util::HelpFormatter;
using Poco::Util::Option;
using Poco::Util::OptionCallback;
using Poco::Util::OptionSet;
using Poco::Util::ServerApplication;


class MessengerHandler : public HTTPRequestHandler
{
public:
    MessengerHandler() : _session(nullptr) {}
    
    void handleRequest(HTTPServerRequest& request, HTTPServerResponse& response) override
    {
        try
        {
            // Получение сессии с базой данных
            if (!_session)
            {
                Poco::Data::PostgreSQL::Connector::registerConnector();
                _session = new Session("PostgreSQL", "host=db dbname=messenger_db user=postgres password=password");
            }
            
            // Разбор URI запроса
            std::string uri = request.getURI();
            std::vector<std::string> parts;
            Poco::Net::MessageHeader::splitElements(uri, parts, '/');
            
            if (parts.size() >= 2)
            {
                std::string resource = parts[1];
                
                if (resource == "users")
                {
                    if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_GET)
                    {
                        if (parts.size() == 2)
                        {
                            // Получение списка всех пользователей
                            try
                            {
                                Poco::JSON::Array users;
                                Statement select(*_session);
                                select << "SELECT id, login, first_name, last_name FROM users",
                                    into(users),
                                    now;
                                
                                Poco::JSON::Object result;
                                result.set("users", users);
                                
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                                response.setContentType("application/json");
                                std::ostream& ostr = response.send();
                                Poco::JSON::Stringifier::stringify(result, ostr);
                            }
                            catch (Poco::Exception& exc)
                            {
                                std::cerr << exc.displayText() << std::endl;
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                                response.send();
                            }
                        }
                        else if (parts.size() == 3)
                        {
                            // Получение пользователя по id
                            try
                            {
                                int user_id = std::stoi(parts[2]);
                                
                                Poco::JSON::Object user;
                                Statement select(*_session);
                                select << "SELECT id, login, first_name, last_name FROM users WHERE id = ?",
                                    into(user),
                                    use(user_id),
                                    now;
                                
                                if (user.size() > 0)
                                {
                                    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                                    response.setContentType("application/json");
                                    std::ostream& ostr = response.send();
                                    Poco::JSON::Stringifier::stringify(user, ostr);
                                }
                                else
                                {
                                    response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
                                    response.send();
                                }
                            }
                            catch (Poco::Exception& exc)
                            {
                                std::cerr << exc.displayText() << std::endl;
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                                response.send();
                            }
                        }
                        else if (parts.size() == 3 && parts[2] == "login")
                        {
                            // Поиск пользователя по логину
                            try
                            {
                                std::string login = request.get("login");
                                
                                Poco::JSON::Object user;
                                Statement select(*_session);
                                select << "SELECT id, login, first_name, last_name FROM users WHERE login = ?",
                                    into(user),
                                    use(login),
                                    now;
                                
                                if (user.size() > 0)
                                {
                                    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                                    response.setContentType("application/json");
                                    std::ostream& ostr = response.send();
                                    Poco::JSON::Stringifier::stringify(user, ostr);
                                }
                                else
                                {
                                    response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
                                    response.send();
                                }
                            }
                            catch (Poco::Exception& exc)
                            {
                                std::cerr << exc.displayText() << std::endl;
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                                response.send();
                            }
                        }
                        else if (parts.size() == 4 && parts[2] == "search")
                        {
                            // Поиск пользователей по логину, имени и фамилии
                            try
                            {
                                std::string query = parts[3];
                                
                                Poco::JSON::Array users;
                                std::string pattern = "%" + query + "%";
                                Statement select(*_session);
                                select << "SELECT id, login, first_name, last_name FROM users WHERE login LIKE ? OR first_name LIKE ? OR last_name LIKE ?",
                                    into(users),
                                    use(pattern),
                                    use(pattern),
                                    use(pattern),
                                    now;
                                
                                Poco::JSON::Object result;
                                result.set("users", users);
                                
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                                response.setContentType("application/json");
                                std::ostream& ostr = response.send();
                                Poco::JSON::Stringifier::stringify(result, ostr);
                            }
                            catch (Poco::Exception& exc)
                            {
                                std::cerr << exc.displayText() << std::endl;
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                                response.send();
                            }
                        }
                        else
                        {
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                            response.send();
                        }
                    }
                    else if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_POST)
                    {
                        // Создание нового пользователя
                        try
                        {
                            // Poco::JSON::Object user;
                            // Poco::JSON::Parser parser;
                            // parser.parse(request.stream(), user);

                            Poco::JSON::Parser parser;
                            Poco::Dynamic::Var result = parser.parse(request.stream());
                            Poco::JSON::Object::Ptr user = result.extract<Poco::JSON::Object::Ptr>();
                            
                            // std::string login = user.getValue<std::string>("login");
                            // std::string password = user.getValue<std::string>("password");
                            // std::string first_name = user.getValue<std::string>("first_name");
                            // std::string last_name = user.getValue<std::string>("last_name");

                            std::string login = user->getValue<std::string>("login");
                            std::string password = user->getValue<std::string>("password");
                            std::string first_name = user->getValue<std::string>("first_name");
                            std::string last_name = user->getValue<std::string>("last_name");
                            
                            // Хэширование пароля перед сохранением
                            std::string password_hash = hashPassword(password);
                            // TODO: реализовать хэширование пароля
                            
                            Statement insert(*_session);
                            insert << "INSERT INTO users (login, password_hash, first_name, last_name) VALUES (?, ?, ?, ?)",
                                use(login),
                                use(password_hash),
                                use(first_name),
                                use(last_name),
                                now;
                            
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_CREATED);
                            response.send();
                        }
                        catch (Poco::Exception& exc)
                        {
                            std::cerr << exc.displayText() << std::endl;
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                            response.send();
                        }
                    }
                    else if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_PUT)
                    {
                        // Обновление данных пользователя
                        try
                        {
                            int user_id = std::stoi(parts[2]);
                            
                            // Poco::JSON::Object user;
                            // Poco::JSON::Parser parser;
                            // parser.parse(request.stream(), user);

                            Poco::JSON::Parser parser;
                            Poco::Dynamic::Var result = parser.parse(request.stream());
                            Poco::JSON::Object::Ptr user = result.extract<Poco::JSON::Object::Ptr>();
                            
                            // std::string login = user.getValue<std::string>("login");
                            // std::string first_name = user.getValue<std::string>("first_name");
                            // std::string last_name = user.getValue<std::string>("last_name");

                            std::string login = user->getValue<std::string>("login");
                            std::string first_name = user->getValue<std::string>("first_name");
                            std::string last_name = user->getValue<std::string>("last_name");
                            
                            Statement update(*_session);
                            update << "UPDATE users SET login = ?, first_name = ?, last_name = ? WHERE id = ?",
                                use(login),
                                use(first_name),
                                use(last_name),
                                use(user_id),
                                now;
                            
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                            response.send();
                        }
                        catch (Poco::Exception& exc)
                        {
                            std::cerr << exc.displayText() << std::endl;
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                            response.send();
                        }
                    }
                    else if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_DELETE)
                    {
                        // Удаление пользователя
                        try
                        {
                            int user_id = std::stoi(parts[2]);
                            
                            Statement delete_(*_session);
                            delete_ << "DELETE FROM users WHERE id = ?",
                                use(user_id),
                                now;
                            
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                            response.send();
                        }
                        catch (Poco::Exception& exc)
                        {
                            std::cerr << exc.displayText() << std::endl;
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                            response.send();
                        }
                    }
                    else
                    {
                        response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                        response.send();
                    }
                }
                else if (resource == "chats")
                {
                    if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_GET)
                    {
                        if (parts.size() == 2)
                        {
                            // Получение списка всех чатов
                            try
                            {
                                Poco::JSON::Array chats;
                                Statement select(*_session);
                                select << "SELECT id, name, is_group_chat FROM chats",
                                    into(chats),
                                    now;
                                
                                Poco::JSON::Object result;
                                result.set("chats", chats);
                                
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                                response.setContentType("application/json");
                                std::ostream& ostr = response.send();
                                Poco::JSON::Stringifier::stringify(result, ostr);
                            }
                            catch (Poco::Exception& exc)
                            {
                                std::cerr << exc.displayText() << std::endl;
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                                response.send();
                            }
                        }
                        else if (parts.size() == 3)
                        {
                            // Получение чата по id
                            try
                            {
                                int chat_id = std::stoi(parts[2]);
                                
                                Poco::JSON::Object chat;
                                Statement select(*_session);
                                select << "SELECT id, name, is_group_chat FROM chats WHERE id = ?",
                                    into(chat),
                                    use(chat_id),
                                    now;
                                
                                if (chat.size() > 0)
                                {
                                    response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                                    response.setContentType("application/json");
                                    std::ostream& ostr = response.send();
                                    Poco::JSON::Stringifier::stringify(chat, ostr);
                                }
                                else
                                {
                                    response.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
                                    response.send();
                                }
                            }
                            catch (Poco::Exception& exc)
                            {
                                std::cerr << exc.displayText() << std::endl;
                                response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                                response.send();
                            }
                        }
                        else
                        {
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                            response.send();
                        }
                    }
                    else if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_POST)
                    {
                        // Создание нового чата
                        try
                        {
                            // Poco::JSON::Object chat;
                            // Poco::JSON::Parser parser;
                            // parser.parse(request.stream(), chat);

                            Poco::JSON::Parser parser;
                            Poco::Dynamic::Var result = parser.parse(request.stream());
                            Poco::JSON::Object::Ptr chat = result.extract<Poco::JSON::Object::Ptr>();
                            
                            // std::string name = chat.getValue<std::string>("name");
                            // bool is_group_chat = chat.getValue<bool>("is_group_chat");

                            std::string name = chat->getValue<std::string>("name");
                            bool is_group_chat = chat->getValue<bool>("is_group_chat");
                            
                            Statement insert(*_session);
                            insert << "INSERT INTO chats (name, is_group_chat) VALUES (?, ?)",
                                use(name),
                                use(is_group_chat),
                                now;
                            
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_CREATED);
                            response.send();
                        }
                        catch (Poco::Exception& exc)
                        {
                            std::cerr << exc.displayText() << std::endl;
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                            response.send();
                        }
                    }
                    else if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_POST && parts.size() == 4 && parts[3] == "members")
                    {
                        // Добавление пользователя в чат
                        try
                        {
                            int chat_id = std::stoi(parts[2]);
                            
                            // Poco::JSON::Object member;
                            // Poco::JSON::Parser parser;
                            // parser.parse(request.stream(), member);

                            Poco::JSON::Parser parser;
                            Poco::Dynamic::Var result = parser.parse(request.stream());
                            Poco::JSON::Object::Ptr member = result.extract<Poco::JSON::Object::Ptr>();
                            
                            // int user_id = member.getValue<int>("user_id");

                            int user_id = member->getValue<int>("user_id");
                            
                            Statement insert(*_session);
                            insert << "INSERT INTO user_chats (user_id, chat_id) VALUES (?, ?)",
                                use(user_id),
                                use(chat_id),
                                now;
                            
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_CREATED);
                            response.send();
                        }
                        catch (Poco::Exception& exc)
                        {
                            std::cerr << exc.displayText() << std::endl;
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                            response.send();
                        }
                    }
                    else
                    {
                        response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                        response.send();
                    }
                }
                else if (resource == "messages")
                {
                    if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_GET && parts.size() == 4 && parts[2] == "private")
                    {
                        // Получение PtP списка сообщений для пользователя
                        try
                        {
                            int user_id = std::stoi(parts[3]);
                            
                            Poco::JSON::Array messages;
                            Statement select(*_session);
                            select << "SELECT m.id, m.sender_id, m.chat_id, m.content, m.timestamp "
                                    "FROM messages m "
                                    "JOIN user_chats uc ON m.chat_id = uc.chat_id "
                                    "WHERE uc.user_id = ? AND uc.chat_id IN (SELECT id FROM chats WHERE is_group_chat = false)",
                                into(messages),
                                use(user_id),
                                now;
                            
                            Poco::JSON::Object result;
                            result.set("messages", messages);
                            
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                            response.setContentType("application/json");
                            std::ostream& ostr = response.send();
                            Poco::JSON::Stringifier::stringify(result, ostr);
                        }
                        catch (Poco::Exception& exc)
                        {
                            std::cerr << exc.displayText() << std::endl;
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                            response.send();
                        }
                    }
                    else if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_GET)
                    {
                        // Получение сообщений чата
                        try
                        {
                            int chat_id = std::stoi(parts[2]);
                            
                            Poco::JSON::Array messages;
                            Statement select(*_session);
                            select << "SELECT id, sender_id, chat_id, content, timestamp FROM messages WHERE chat_id = ?",
                                into(messages),
                                use(chat_id),
                                now;
                            
                            Poco::JSON::Object result;
                            result.set("messages", messages);
                            
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                            response.setContentType("application/json");
                            std::ostream& ostr = response.send();
                            Poco::JSON::Stringifier::stringify(result, ostr);
                        }
                        catch (Poco::Exception& exc)
                        {
                            std::cerr << exc.displayText() << std::endl;
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                            response.send();
                        }
                    }
                    else if (request.getMethod() == Poco::Net::HTTPRequest::HTTP_POST)
                    {
                        // Отправка сообщения в чат
                        try
                        {
                            // Poco::JSON::Object message;
                            // Poco::JSON::Parser parser;
                            // parser.parse(request.stream(), message);

                            Poco::JSON::Parser parser;
                            Poco::Dynamic::Var result = parser.parse(request.stream());
                            Poco::JSON::Object::Ptr message = result.extract<Poco::JSON::Object::Ptr>();
                            
                            // int sender_id = message.getValue<int>("sender_id");
                            // int chat_id = message.getValue<int>("chat_id");
                            // std::string content = message.getValue<std::string>("content");

                            int sender_id = message->getValue<int>("sender_id");
                            int chat_id = message->getValue<int>("chat_id");
                            std::string content = message->getValue<std::string>("content");
                            
                            Statement insert(*_session);
                            insert << "INSERT INTO messages (sender_id, chat_id, content) VALUES (?, ?, ?)",
                                use(sender_id),
                                use(chat_id),
                                use(content),
                                now;
                            
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_CREATED);
                            response.send();
                        }
                        catch (Poco::Exception& exc)
                        {
                            std::cerr << exc.displayText() << std::endl;
                            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
                            response.send();
                        }
                    }
                    else
                    {
                        response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                        response.send();
                    }
                }
                else
                {
                    // Обработка некорректного URI
                    response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                    response.send();
                    return;
                }
            }
            else
            {
                // Обработка некорректного URI
                response.setStatus(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
                response.send();
                return;
            }
        }
        catch (Poco::Exception& exc)
        {
            std::cerr << exc.displayText() << std::endl;
            response.setStatus(Poco::Net::HTTPResponse::HTTP_INTERNAL_SERVER_ERROR);
            response.send();
        }
    }
    
private:
    Session* _session;

    std::string hashPassword(const std::string& password)
    {
        Poco::Crypto::DigestEngine digestEngine("SHA256");
        digestEngine.update(password);
        return Poco::Crypto::DigestEngine::digestToHex(digestEngine.digest());
    }
};


class MessengerHandlerFactory : public HTTPRequestHandlerFactory
{
public:
    MessengerHandlerFactory(const std::string& url) : _url(url) {}
    
    HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) override
    {
        return new MessengerHandler;
    }
    
private:
    std::string _url;
};


class MessengerServer : public ServerApplication
{
public:
    MessengerServer() : _helpRequested(false) {}
    
    ~MessengerServer() {}
    
protected:
    void initialize(Application& self) override
    {
        loadConfiguration();
        ServerApplication::initialize(self);
    }
    
    void defineOptions(OptionSet& options) override
    {
        ServerApplication::defineOptions(options);

        options.addOption(
            Option("help", "h", "Display help information on command line arguments.")
                .required(false)
                .repeatable(false)
                .callback(OptionCallback<MessengerServer>(this, &MessengerServer::handleHelp)));
    }
    
    void handleHelp(const std::string& name, const std::string& value)
    {
        HelpFormatter helpFormatter(options());
        helpFormatter.setCommand(commandName());
        helpFormatter.setUsage("OPTIONS");
        helpFormatter.setHeader("A messenger server application.");
        helpFormatter.format(std::cout);
        stopOptionsProcessing();
        _helpRequested = true;
    }
    
    int main(const std::vector<std::string>& args) override
    {
        if (!_helpRequested)
        {
            unsigned short port = (unsigned short)config().getInt("MessengerServer.port", 8080);

            ServerSocket svs(port);
            HTTPServer srv(new MessengerHandlerFactory("/"), svs, new HTTPServerParams);

            srv.start();
            waitForTerminationRequest();
            srv.stop();
        }
        return Application::EXIT_OK;
    }
    
private:
    bool _helpRequested;
};


int main(int argc, char** argv)
{
    MessengerServer app;
    return app.run(argc, argv);
}