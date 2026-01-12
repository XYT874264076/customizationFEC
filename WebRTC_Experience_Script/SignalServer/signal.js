const net = require('net');

const HOST = '192.168.0.1';
const PORT = 3000;

let room = [];

const server = net.createServer((socket) => {
  console.log('New connection established');

  socket.on('data', (data) => {
    const message = data.toString().trim();

    if (message === '/join') {
      room.push(socket);
      console.log('User joined the room');

      // Notify the current user
      socket.write('/joined\n');

      // Notify other users in the room
      room.forEach((client) => {
        if (client !== socket) {
          client.write('/other_join\n');
        }
      });
    } else if (message === '/leave') {
      room = room.filter((client) => client !== socket);
      console.log('User left the room');

      // Notify the current user
      socket.write('/leaved\n');

      // Notify other users in the room
      room.forEach((client) => {
        client.write('/other_leave\n');
      });
    } else if (message.startsWith('/message')) {
      console.log('Message received:', message);

      // Forward the message to other users in the room
      room.forEach((client) => {
        if (client !== socket) {
          client.write(message + '\n');
        }
      });
    }
  });

  socket.on('end', () => {
    room = room.filter((client) => client !== socket);
    console.log('Connection closed');
  });

  socket.on('error', (err) => {
    console.error('Socket error:', err);
  });
});

// Send /wait message to all users in the room every 10 seconds
setInterval(() => {
  room.forEach((client) => {
    client.write('/wait\n');
  });
}, 10000);

server.listen(PORT, HOST, () => {
  console.log(`TCP server is running on tcp://${HOST}:${PORT}`);
});
