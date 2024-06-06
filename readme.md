<p align="center">
  <img src="https://github.com/AaryaDevnani/TribesNetworkingModel/assets/62675730/800544b3-1dfc-4fd3-9726-ec4ea7846d95" alt="Tank Racer Gif" style="width: 976;"/>
</p>
<p align="center">
Racing Game Demo
</p>

# Prime Engine

Prime Engine is an engine developed by Prof. Artem Kovalovs.

I had taken a course on Game Engine Development under Professor Kovalovs in Spring ‘24.

Prime Engine is written entirely by Professor Kovalovs. 

The assignment for the course involved implementing features into this engine. This repo contains a snippet of some functionalities that I have added to the engine.

# Tribes Networking Model

Professor Kovalovs had implemented the Tribes Networking Model using TCP sockets.
- Here is a brief overview of the Architecture:
    - We have 2 main entities:
        1. Server Network Manager
        2. Client Network Manager
    - A Client starts by sending out a connection request to the server.
    - Once the server receives this request, it does the following:
        1. Creates a new socket for the for the Server instance specific to this client (Each client has a specific socket to communicate with the server.)
        2. Sends an acknowledgement along with the new server port and address to the client.
        3. On receiving this acknowledgement, the client sends a connection message identifying themselves to this newly created socket.
        4. Any communication that takes place between the Main Server and the client is via the Client specific server.
- Here are some of the features that I was able to add to this implementation along with video demos for the same:
    1. [Force Packet Drops & Debug view for these packet drops](https://drive.google.com/file/d/1MmkdRtvrvD6OZEOC3hbbKHaS_P9ea8ge/view?usp=drive_link)
    2. [Convert the existing system into using UDP sockets](https://drive.google.com/file/d/1kXJRcKUOxbuPSgQvSa42X1daKqYEpX7v/view?usp=sharing)
    3. [Implemented a small multiplayer racing game demo](https://drive.google.com/file/d/1dosqXal6ea-Ni7T1OdAolp9c78b5h3VT/view?usp=sharing)
