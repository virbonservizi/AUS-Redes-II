// handlers.c

#include "responses.h"
#include "pi.h"
#include "dtp.h"
#include "session.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

void handle_USER(const char *args) {
  ftp_session_t *sess = session_get();

  if (!args || strlen(args) == 0) {
    safe_dprintf(sess->control_sock, MSG_501); // Syntax error in parameters
    return;
  }

  strncpy(sess->current_user, args, sizeof(sess->current_user) - 1);
  sess->current_user[sizeof(sess->current_user) - 1] = '\0';
  safe_dprintf(sess->control_sock, MSG_331); // Username okay, need password
}

void handle_PASS(const char *args) {

  ftp_session_t *sess = session_get();

  if (sess->current_user[0] == '\0') {
    safe_dprintf(sess->control_sock, MSG_503); // Bad sequence of commands
    return;
  }

  if (!args || strlen(args) == 0) {
    safe_dprintf(sess->control_sock, MSG_501); // Syntax error in parameters
    return;
  }

  if (check_credentials(sess->current_user, (char *)args) == 0) {
    sess->logged_in = 1;
    safe_dprintf(sess->control_sock, MSG_230); // User logged in
  } else {
    safe_dprintf(sess->control_sock, MSG_530); // Not logged in
    sess->current_user[0] = '\0'; // Reset user on failed login
    sess->logged_in = 0;
  }
}

void handle_QUIT(const char *args) {
  ftp_session_t *sess = session_get();
  (void)args; // unused

  safe_dprintf(sess->control_sock, MSG_221); // 221 Goodbye.
  sess->current_user[0] = '\0'; // Close session
  close_fd(sess->control_sock, "client socket"); // Close socket
  sess->control_sock = -1;
}

void handle_SYST(const char *args) {
  ftp_session_t *sess = session_get();
  (void)args; // unused

  safe_dprintf(sess->control_sock, MSG_215); // 215 <system type>
}

void handle_TYPE(const char *args) {

  ftp_session_t *sess = session_get();

  if (!sess->logged_in) {
    safe_dprintf(sess->control_sock, MSG_530); // 530 Not logged in
    return;
  }

  if (!args || strlen(args) == 0) {
    // Default to Image/Binary
    safe_dprintf(sess->control_sock, "Type set to binary.\r\n");
    safe_dprintf(sess->control_sock, MSG_200); // 200 Command okay
    return;
  }

  // We only support binary mode (I) for now, for that reason we do not keep track of the type
  if (args[0] != 'I') {
    safe_dprintf(sess->control_sock, MSG_504); // 504 Command not implemented for that parameter
    return;
  }

  safe_dprintf(sess->control_sock, "Type set to binary.\r\n");
  safe_dprintf(sess->control_sock, MSG_200); // 200 Command okay
}

void handle_PORT(const char *args) {
  ftp_session_t *sess = session_get();

  if (!sess->logged_in) {
    safe_dprintf(sess->control_sock, MSG_530); // 530 Not logged in
    return;
  }

  if (!args || strlen(args) < 11) {
    safe_dprintf(sess->control_sock, MSG_501); // 501 Syntax error in parameters
    return;
  }

  int h1, h2, h3, h4, p1, p2;
  if (sscanf(args, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
    safe_dprintf(sess->control_sock, MSG_501); // 501 Syntax error in parameters
    return;
  }

  char ip_str[INET_ADDRSTRLEN];
  snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", h1, h2, h3, h4);
  sess->data_addr.sin_family = AF_INET;
  sess->data_addr.sin_port = htons(p1 * 256 + p2);

  if (inet_pton(AF_INET, ip_str, &sess->data_addr.sin_addr) <= 0) {
    safe_dprintf(sess->control_sock, MSG_501); // 501 Syntax error in parameters
    return;
  }

  safe_dprintf(sess->control_sock, MSG_200);
}

void handle_RETR(const char *args) {
  ftp_session_t *sess = session_get();

  if (!sess->logged_in) {
    safe_dprintf(sess->control_sock, MSG_530); // 530 Not logged in
    return;
  }

  if (!args || strlen(args) == 0) {
    safe_dprintf(sess->control_sock, MSG_501); // 501 Syntax error in parameters
    return;
  }

  if (sess->data_addr.sin_port == 0) {
    // For now, we require a PORT command before RETR
    safe_dprintf(sess->control_sock, MSG_503); // 503 Bad sequence of commands
    return;
  }

  // Open the file for reading
  int file_fd = open(args, O_RDONLY);
  if (file_fd < 0) {
    safe_dprintf(sess->control_sock, MSG_550, args); // 550 Requested action not taken; file unavailable
    return;
  }

  // Create a data socket
  int data_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (data_sock < 0) {
    safe_dprintf(sess->control_sock, MSG_425); // 425 Can't open data connection
    close(file_fd);
    return;
  }

  if (connect(data_sock, (struct sockaddr *)&sess->data_addr, sizeof(sess->data_addr)) < 0) {
    safe_dprintf(sess->control_sock, MSG_425); // 425 Can't open data connection
    fprintf(stderr, "Falló la conexión de datos:\n");
    perror(NULL);
    close(file_fd);
    close(data_sock);
    return;
  }

  safe_dprintf(sess->control_sock, MSG_150); // 150 File status okay; about to open data connection.

  // Send the file content over the data connection
  char buf[1024];
  ssize_t bytes;
  while ((bytes = read(file_fd, buf, sizeof(buf))) > 0) {
    if (send(data_sock, buf, bytes, 0) != bytes) {
      fprintf(stderr, "Envio parcial:\n");
      perror(NULL);
      break;
    }
  }

  close(file_fd);
  close(data_sock);

  safe_dprintf(sess->control_sock, MSG_226); // 226 Transfer complete
}

void handle_STOR(const char *args) {
  ftp_session_t *sess = session_get();

  if (!sess->logged_in) {
    safe_dprintf(sess->control_sock, MSG_530); // 530 Not logged in
    return;
  }

  if (!args || strlen(args) == 0) {
    safe_dprintf(sess->control_sock, MSG_501); // 501 Syntax error in parameters
    return;
  }

  if (sess->data_addr.sin_port == 0) {
    // For now, we require a PORT command before STOR
    safe_dprintf(sess->control_sock, MSG_503); // 503 Bad sequence of commands
    return;
  }

  // Create a data socket
  int data_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (data_sock < 0) {
    safe_dprintf(sess->control_sock, MSG_425); // 425 Can't open data connection
    return;
  }

  if (connect(data_sock, (struct sockaddr *)&sess->data_addr, sizeof(sess->data_addr)) < 0) {
    fprintf(stderr, "Falló la conexión de datos:\n");    
    perror(NULL);
    safe_dprintf(sess->control_sock, MSG_425); // 425 Can't open data connection
    close(data_sock);
    return;
  }

  // Open the file for writing
  int file_fd = open(args, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
      safe_dprintf(sess->control_sock, MSG_550, args); // 550 Requested action not taken; file unavailable
      close(data_sock);
      return;
    }

  safe_dprintf(sess->control_sock, MSG_150); // 150 File status okay; about to open data connection.

  // Receive the file content over the data connection
  char buf[1024];
  ssize_t bytes;
  while ((bytes = recv(data_sock, buf, sizeof(buf), 0)) > 0) {
    if (write(file_fd, buf, bytes) != bytes) {
      fprintf(stderr, "Error al escribir en el archivo:\n");
      perror(NULL);
      break;
    }
  }

  close(file_fd);
  close(data_sock);

  safe_dprintf(sess->control_sock, MSG_226); // 226 Transfer complete
}

void handle_NOOP(const char *args) {
  ftp_session_t *sess = session_get();
  (void)args;
  (void)sess;

  safe_dprintf(sess->control_sock, MSG_200); // 200 Command okay
}
