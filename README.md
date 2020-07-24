# Private Messenger



### Project Type

- [x] University Project
- [ ] Self Challenge - Learning New Things
- [ ] Project for a Company 
- [x] Team work



### Task to accomplish

Implement a system where it is possible for each user to leave messages to other users. To do this, you must produce a server managed by an administrator. He has the responsibility to add and delete users.
To access the server, the administrator and users must identify themselves by correctly entering their login and password. Once registered, each user can:
- Check if there are messages sent by other users.
- Read messages.
- Send a message to another user.
- Send the same message to multiple users.
- Delete already read messages.

You only need to deploy the server side. For the client program you can use the Telnet.



### Show Project

Login for the first time:



Login as the Administrator:



Login as the Client:





### Project summary

Whenever the program runs for the first time, the first thing that is done is check whether or not a database exists. If the database is not found, then a folder will be created with the name "Raiz" (meaning: root) where all of the text files, that work as message boxes for each user, will be stored. As well as the lists with the basic information of each user.

After the login has been successful, the user can choose, between a list of options, what action he wishes to perform. 

The client can choose to:

* See if he has unread messages
* View the list of messages and open one, if he wishes
* It is possible to delete one or more messages already seen
* Send messages to one or more users
* Change the password
* Exit

While the administrator can do the following:

* View the list of people looking for admission and accept or refuse someone
* Delete user access
* Allow the admin to use normal services, just like usual client
* Exit



### What I learned?

* How to program using the C language
* Programming using sockets to establish the communication between the client Telnet and the server program
* New insights of how the internet works and learning about network architecture



### How to run the project locally?

First you need to have the telnet instaled.

The application runs on port 9000, so, you will need to write the folliwing commands, one for each of the two command lines (in this order):

* Server terminal: compile the program with `gcc -o new projeto_servidor.c` and run the program `new`

* Cliente terminal: `telnet 127.0.0.1 9000`

  

Whenever a new customer tries to register into the system for the first time, the admin will have to give authorization. Only after this is done, will the new cliente be able to use the program.

So, after registering a new customer, you will have to enter the system with:

* Username: admin
* Password: 123

And accept that particular client.



### Warnings and/or Disclaimers

* Many things in this project were written in Portuguese



