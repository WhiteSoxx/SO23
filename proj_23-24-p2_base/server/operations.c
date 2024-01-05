#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include "common/io.h"
#include "eventlist.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_us = 0;

void safe_open(int fd) {
  if(fd == -1) {
    fprintf(stderr, "[Err]: open failed: %d\n",(errno));
    exit(EXIT_FAILURE);
  }
}


void safe_write(int fd, void *session_id, size_t count) {
  ssize_t bytes_written;
    do {
        bytes_written = write(fd, session_id, count);
    } while (bytes_written < 0 && errno == EINTR);
    return bytes_written;
}


void safe_read(int fd, void* operation_code, size_t count) {
  ssize_t bytes_read;
    do {
        bytes_read = read(fd, operation_code, count);
    } while (bytes_read < 0 && errno == EINTR);
    return bytes_read;
}


int status_signal() {
    for (struct ListNode* current = event_list->head; current != NULL; current = current->next) {
      printf("Event: %u\n", ((struct Event*)current->event)->id);
      
      if (event_list == NULL) {
        fprintf(stderr, "EMS state must be initialized\n");
        return 1;
      }

      if (current == NULL) {
        fprintf(stderr, "Event not found\n");
        return 1;
      }

      pthread_rwlock_wrlock(&event_list->rwl);
      for (size_t i = 1; i <= current->event->rows; i++) {
        for (size_t j = 1; j <= current->event->cols; j++) {
          char line_buffer[16];

          sprintf(line_buffer, "%u", current->event->data[(i - 1) * current->event->cols + j - 1]);

          if (print_str(STDOUT_FILENO, line_buffer)) {
            perror("Error writing to file descriptor");
            return 1;
          }

          if (j < current->event->cols) {
            if (print_str(STDOUT_FILENO, " ")) {
              perror("Error writing to file descriptor");
              return 1;
            }
          }
        }

        if (print_str(STDOUT_FILENO, "\n")) {
          perror("Error writing to file descriptor");
          return 1;
        } 
      }
      pthread_rwlock_unlock(&event_list->rwl);
    }
    return 0;
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @param from First node to be searched.
/// @param to Last node to be searched.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id, struct ListNode* from, struct ListNode* to) {
  struct timespec delay = {0, state_access_delay_us * 1000};
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id, from, to);
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }


int ems_init(unsigned int delay_us) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_us = delay_us;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  free_list(event_list);
  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}


int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_wrlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  if (get_event_with_delay(event_id, event_list->head, event_list->tail) != NULL) {
    fprintf(stderr, "Event already exists\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    fprintf(stderr, "Error allocating memory for event\n");
    pthread_rwlock_unlock(&event_list->rwl);
    return 1;
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  if (pthread_mutex_init(&event->mutex, NULL) != 0) {
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }
  event->data = calloc(num_rows * num_cols, sizeof(unsigned int));

  if (event->data == NULL) {
    fprintf(stderr, "Error allocating memory for event data\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event);
    return 1;
  }

  if (append_to_list(event_list, event) != 0) {
    fprintf(stderr, "Error appending event to list\n");
    pthread_rwlock_unlock(&event_list->rwl);
    free(event->data);
    free(event);
    return 1;
  }

  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}


int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  for (size_t i = 0; i < num_seats; i++) {
    if (xs[i] <= 0 || xs[i] > event->rows || ys[i] <= 0 || ys[i] > event->cols) {
      fprintf(stderr, "Seat out of bounds\n");
      pthread_mutex_unlock(&event->mutex);
      return 1;
    }
  }

  for (size_t i = 0; i < event->rows * event->cols; i++) {
    for (size_t j = 0; j < num_seats; j++) {
      if (seat_index(event, xs[j], ys[j]) != i) {
        continue;
      }

      if (event->data[i] != 0) {
        fprintf(stderr, "Seat already reserved\n");
        pthread_mutex_unlock(&event->mutex);
        return 1;
      }

      break;
    }
  }

  unsigned int reservation_id = ++event->reservations;

  for (size_t i = 0; i < num_seats; i++) {
    event->data[seat_index(event, xs[i], ys[i])] = reservation_id;
  }

  pthread_mutex_unlock(&event->mutex);
  return 0;
}


int ems_show(int out_fd, unsigned int event_id) { //isto tem de ser com um buffer por causa do int retorno FIXME
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  int status = 0;
  write(out_fd, &status, sizeof(unsigned int));

  struct Event* event = get_event_with_delay(event_id, event_list->head, event_list->tail);

  pthread_rwlock_unlock(&event_list->rwl);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  if (pthread_mutex_lock(&event->mutex) != 0) {
    fprintf(stderr, "Error locking mutex\n");
    return 1;
  }

  unsigned int* seats_array = malloc(event->rows * event->cols * sizeof(unsigned int));
  write(out_fd, &event->rows, sizeof(size_t));
  write(out_fd, &event->cols, sizeof(size_t));
  write(out_fd, event->data, event->rows * event->cols * sizeof(unsigned int));

  pthread_mutex_unlock(&event->mutex);
  return 0;
}


int ems_list_events(int out_fd) {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (pthread_rwlock_rdlock(&event_list->rwl) != 0) {
    fprintf(stderr, "Error locking list rwl\n");
    return 1;
  }

  int status = 0;
  size_t num_events = 0;
  write(out_fd, &status, sizeof(unsigned int));

  unsigned int* ids = malloc(sizeof(unsigned int));
  for (struct ListNode* current = event_list->head; current != NULL; current = current->next) {
    ids[num_events] = ((struct Event*)current->event)->id;
    num_events++;
    realloc(ids, (num_events + 1) * sizeof(unsigned int));
  }

  write(out_fd, &num_events, sizeof(size_t));
  write(out_fd, ids, num_events * sizeof(unsigned int));

  pthread_rwlock_unlock(&event_list->rwl);
  return 0;
}