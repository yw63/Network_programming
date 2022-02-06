#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <memory>
#include <utility>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>

using boost::asio::ip::tcp;
using namespace boost::asio;
using namespace std;

struct Param {
    string host, port, file;
} param;

vector<struct Param> params;

void parser() {
    string query;
    query = string(getenv("QUERY_STRING"));
    for(int i = 0; i < 5; i++) {
        if(query[query.find("=") + 1] == '&') break;
        query = query.substr(query.find("=") + 1);
        param.host = query.substr(0, query.find("&"));
        query = query.substr(query.find("=") + 1);
        param.port = query.substr(0, query.find("&"));
        query = query.substr(query.find("=") + 1);
        param.file = query.substr(0, query.find("&"));
        params.push_back(param);
    }
}

void print_head() { 
    for(auto it = params.begin(); it != params.end(); it++)
        cout << "<th scope=\"col\">" << it->host << ":" << it->port << "</th>\n";
}

void print_body() {
    for(size_t i = 0; i < params.size(); i++)
        cout << "<td><pre id=\"s" << i << "\" class=\"mb-0\"></pre></td>\n";
}

void print_html() {

	cout << "Content-type: text/html\r\n\r\n";

    cout << "<!DOCTYPE html>\n";
    cout << "<html lang=\"en\">\n";
    	cout << "<head>\n";
	        cout << "<meta charset=\"UTF-8\" />\n";
	        cout << "<title>NP Project 3 Console</title>\n";
	        cout << "<link\n";
		        cout << "rel=\"stylesheet\"\n";
		        cout << "href=\"https://cdn.jsdelivr.net/npm/bootstrap@4.5.3/dist/css/bootstrap.min.css\"\n";
		        cout << "integrity=\"sha384-TX8t27EcRE3e/ihU7zmQxVncDAy5uIKz4rEkgIXeMed4M0jlfIDPvg6uqKI2xXr2\"\n";
		        cout << "crossorigin=\"anonymous\"\n";
        	cout << "/>\n";
        	cout << "<link\n";
		        cout << "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n";
		        cout << "rel=\"stylesheet\"\n";
	        cout << "/>\n";
        	cout << "<link\n";
		        cout << "rel=\"icon\"\n";
		        cout << "type=\"image/png\"\n";
		        cout << "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n";
        	cout << "/>\n";
        	cout << "<style>\n";
		        cout << "* {\n";
		            cout << "font-family: \'Source Code Pro\', monospace;\n";
		            cout << "font-size: 1rem !important;\n";
		        cout << "}\n";
		        cout << "body {\n";
		            cout << "background-color: #212529;\n";
		        cout << "}\n";
		        cout << "pre {\n";
		            cout << "color: #cccccc;\n";
		        cout << "}\n";
		        cout << "b {\n";
		            cout << "color: #01b468;\n";
		        cout << "}\n";
        	cout << "</style>\n";
    	cout << "</head>\n";
    	cout << "<body>\n";
        	cout << "<table class=\"table table-dark table-bordered\">\n";
        		cout << "<thead>\n";
            		cout << "<tr>\n";
            			print_head();
            		cout << "</tr>\n";
        		cout << "</thead>\n";
        		cout << "<tbody>\n";
            		cout << "<tr>\n";
            			print_body();
            		cout << "</tr>\n";
        		cout << "</tbody>\n";
        	cout << "</table>\n";
    	cout << "</body>\n";
    cout << "</html>\n";
}

void escape_html(string& s) {
    boost::algorithm::replace_all(s, "\n", "&NewLine;");
    boost::algorithm::replace_all(s, "\r", "");
    boost::algorithm::replace_all(s, "\'", "&apos;");
    boost::algorithm::replace_all(s, "\"", "&quot;");
    boost::algorithm::replace_all(s, "<", "&lt;");
    boost::algorithm::replace_all(s, ">", "&gt;");
}

void print_sess(string s, int id, bool iscmd) {
    escape_html(s);
    if(iscmd) {
        s = "<b>" + s + "</b>";
    }
    cout << "<script>document.getElementById('s" << id << "').innerHTML += '" << s << "';</script>" << flush;
}

class Session
: public std::enable_shared_from_this<Session>
{
public:
    Session(io_service& io, string file, int id)
        : resolv(io) , socket_(io), fin("./test_case/" + file), id_(id){}

    void start(tcp::resolver::query query) {
        if(!fin) {
            cerr << "fin error\n";
            return;
        }
        auto self(shared_from_this());
        resolv.async_resolve(query, 
            [this, self](const boost::system::error_code &ec, tcp::resolver::iterator it)
            {
                if(!ec) {
                    auto self = shared_from_this();
                    socket_.async_connect(it->endpoint(),
                    [this, self](boost::system::error_code ec){
                        sess_read();
                    });
                }
            });
    }
private:
    void sess_read() { //read from np golden
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec , size_t length) 
            {
                if (!ec) {
                    string sess_output = string(data_);
                    print_sess(sess_output, id_, false); //print session according to session id
                    bzero(data_, sizeof(data_));
                    if (sess_output.find("% ") != string::npos)
                        sess_write();
                    else
                        sess_read();
                }
            });
    }
    
    void sess_write() { //write command to np golden
        if(!getline(fin, cmd)) return;
        cmd += "\n";
        print_sess(cmd, id_, true);
        auto self(shared_from_this());
        socket_.async_send(buffer(cmd, cmd.size()),
            [this, self](boost::system::error_code ec, size_t length)
            {
                if (!ec) {
                    sess_read();
                }
            });
    }

    tcp::socket socket_;
    tcp::resolver resolv;
    ifstream fin;
    string cmd;
    enum { max_length = 8192 };
    char data_[max_length];
    int id_;
};

int main() {
    parser();
    print_html();

    io_service session_io;
    for(size_t i = 0; i < params.size(); i++) {
        tcp::resolver::query query(params[i].host, params[i].port);
        shared_ptr<Session> s = make_shared<Session>(session_io, params[i].file, i);
        s->start(query);
    }
    session_io.run();

    return 0;
}