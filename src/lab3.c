#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int min(int a, int b) {
  if (a < b) {
    return a;
  } else {
    return b;
  }
}

int max(int a, int b) {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}

void swap(unsigned char **lhs, unsigned char **rhs) {
  unsigned char *tmp = *lhs;
  *lhs = *rhs;
  *rhs = tmp;
}

unsigned char *data = NULL;
int width = 0;
int height = 0;
int comp = 0;
int window_size = 0;

int thread_count = 4;

int filter_by_average(int left_border, int right_border, int lower_border, int upper_border) {
  int sum = 0;
  int count = 0;
  for (int i = lower_border; i <= upper_border; ++i) {
    for (int j = left_border; j <= right_border; j += comp) {
      sum += data[i * width * comp + j];
      ++count;
    }
  }
  return sum / count;
}

int compare(const void *x1, const void *x2) {
  return (*(int *)x1 - *(int *)x2);
}

int filter_by_median(int left_border, int right_border, int lower_border, int upper_border) {
  int size_gor = (right_border - left_border) / comp + 1;
  int size = (upper_border - lower_border + 1) * size_gor;
  int arr[size];
  int arr_idx = 0;
  for (int i = lower_border; i <= upper_border; ++i) {
    for (int j = left_border; j <= right_border; j += comp) {
      arr[arr_idx] = data[i * width * comp + j];
      ++arr_idx;
    }
  }
  qsort(arr, size, sizeof(int), compare);
  return arr[size / 2];
}

int filter(int idx) {
  int real_width = width * comp;
  int left_border = max((idx % real_width) % comp, idx % real_width - window_size * comp);
  int right_border = min(real_width - comp + (idx % real_width) % comp, idx % real_width + window_size * comp);
  int lower_border = max(0, idx / real_width - window_size);
  int upper_border = min(height - 1, idx / real_width + window_size);

  //return filter_by_average(left_border, right_border, lower_border, upper_border);
  return filter_by_median(left_border, right_border, lower_border, upper_border);
}

struct process_buf_args {
  unsigned char *buf;
  int from;
  int to;
};
typedef struct process_buf_args process_buf_args;

void *process_lines(void *args) {
  process_buf_args *ft = (process_buf_args *)args;
  for (int i = ft->from; i < ft->to; ++i) {
    for (int j = 0; j < width * comp; ++j) {
      int idx = i * width * comp + j;
      ft->buf[idx] = filter(idx);
    }
  }
  return NULL;
}

void process_image_to(unsigned char *result) {
  process_buf_args args[thread_count];
  int step = height / thread_count;
  if (step == 0) {
    fprintf(stderr, "The height of the image cannot be greater than the number of threads\n");
    return;
  }
  for (int i = 0; i < thread_count; ++i) {
    args[i].buf = result;
    args[i].from = step * i;
    args[i].to = step * (i + 1);
  }
  args[thread_count - 1].to = height;
  pthread_t ids[thread_count];
  for (int i = 0; i < thread_count; ++i) {
    pthread_create(&ids[i], NULL, process_lines, &args[i]);
  }
  for (int i = 0; i < thread_count; ++i) {
    pthread_join(ids[i], NULL);
  }
}

int main(int argc, char **argv) {
  if (argc < 5 || argc == 6 || argc > 7) {
    fprintf(stderr, "USAGE: %s <input_file> <output_file.png> <count> <window_size>\n", argv[0]);
    fprintf(stderr, "or\nUSAGE: %s <input_file> <output_file.png> <count> <window_size> -t <thread_count>\n", argv[0]);
    return 1;
  }
  if (argc == 7) {
    if (strcmp(argv[5], "-t") == 0 && atoi(argv[6]) > 0) {
      thread_count = atoi(argv[6]);
    } else {
      fprintf(stderr, "Bad thread arrgument\n");
      return 2;
    }
  }
  if (atoi(argv[3]) < 0 || atoi(argv[4]) < 0) {
    fprintf(stderr, "Bad argument 3 or 4\n");
    stbi_image_free(data);
    return 3;
  }

  data = stbi_load(argv[1], &width, &height, &comp, 0);
  if (!data) {
    fprintf(stderr, "Something was wrong during reading\n");
    stbi_image_free(data);
    return 4;
  }
  window_size = atoi(argv[4]);
  fprintf(stderr, "width:%d height:%d channels:%d threads:%d\n", width, height, comp, thread_count);

  time_t start = time(NULL);
  unsigned char *buf = (unsigned char *)malloc(width * height * comp * sizeof(unsigned char));
  for (int k = 0; k < atoi(argv[3]); ++k) {
    process_image_to(buf);
    swap(&data, &buf);
  }
  free(buf);
  time_t finish = time(NULL);
  fprintf(stderr, "time:%lds\n", finish - start);

  int res = stbi_write_png(argv[2], width, height, comp, data, width * comp);
  //int res = stbi_write_bmp(argv[2], width, height, comp, data);
  stbi_image_free(data);

  if (!res) {
    fprintf(stderr, "Something was wrong during writing\n");
    return 5;
  }
  return !res;
}