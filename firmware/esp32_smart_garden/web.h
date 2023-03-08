// Web server of the smart garden: starts the HTTP server, registers the routes
// and answers the client requests. The old readings page and the JSON API used
// by the dashboard are both served from here.

#ifndef WEB_H
#define WEB_H

void webBegin(); // Registers every route, starts the server and prints the state
void webLoop();  // Handles the client requests, call it from loop()

#endif
