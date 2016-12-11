HTTP响应也由三个部分组成，分别是：状态行、消息报头、响应正文。

如下所示，HTTP响应的格式与请求的格式十分类似：

＜status-line＞		
＜headers＞
＜blank line＞		
[＜response-body＞]

正如你所见，在响应中唯一真正的区别在于第一行中用状态信息代替了请求信息。
状态行(status line）通过提供一个状态码来说明所请求的资源情况。

状态行格式如下：

HTTP-Version Status-Code Reason-Phrase CRLF

其中:
HTTP-Version表示服务器HTTP协议的版本；
Status-Code表示服务器发回的响应状态代码；
Reason-Phrase表示状态代码的文本描述。

状态代码由三位数字组成，第一个数字定义了响应的类别，且有五种可能取值。

1xx：指示信息--表示请求已接收，继续处理。
2xx：成功--表示请求已被成功接收、理解、接受。
3xx：重定向--要完成请求必须进行更进一步的操作。
4xx：客户端错误--请求有语法错误或请求无法实现。
5xx：服务器端错误--服务器未能实现合法的请求。

常见状态代码、状态描述的说明如下。
200 OK：客户端请求成功。
400 BadRequest：客户端请求有语法错误，不能被服务器所理解。
401 Unauthorized：请求未经授权，这个状态代码必须和WWW-Authenticate报头域一起使用。
403 Forbidden：服务器收到请求，但是拒绝提供服务。
404 Not Found：请求资源不存在，举个例子：输入了错误的URL。
500 Internal Server Error：服务器发生不可预期的错误。
503 Server Unavailable：服务器当前不能处理客户端的请求，一段时间后可能恢复正常，举个例子：HTTP/1.1 200 OK（CRLF）。



//====================================================================================
English version:

SDK Development Guide @ http://bbs.espressif.com/viewtopic.php?f=51&t=1024
AT Commands User Guide @ http://bbs.espressif.com/viewtopic.php?f=51&t=1022
All documentations @ http://bbs.espressif.com/viewforum.php?f=51

中文文档：

SDK 编程手册 @ http://bbs.espressif.com/viewtopic.php?f=51&t=1023
AT 指令集 @ http://bbs.espressif.com/viewtopic.php?f=51&t=732
全部开发文档 @ http://bbs.espressif.com/viewforum.php?f=51
