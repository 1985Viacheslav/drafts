CREATE TABLE users (
  id SERIAL PRIMARY KEY,
  login VARCHAR(255) UNIQUE NOT NULL,
  password_hash VARCHAR(255) NOT NULL,
  first_name VARCHAR(255) NOT NULL,
  last_name VARCHAR(255) NOT NULL
);

CREATE TABLE chats (
  id SERIAL PRIMARY KEY,
  name VARCHAR(255) NOT NULL,
  is_group_chat BOOLEAN NOT NULL
);

CREATE TABLE user_chats (
  user_id INTEGER REFERENCES users(id),
  chat_id INTEGER REFERENCES chats(id),
  PRIMARY KEY (user_id, chat_id)
);

CREATE TABLE messages (
  id SERIAL PRIMARY KEY,
  sender_id INTEGER REFERENCES users(id),
  chat_id INTEGER REFERENCES chats(id),
  content TEXT NOT NULL,
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);