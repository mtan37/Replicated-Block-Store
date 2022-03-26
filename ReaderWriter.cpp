#include <mutex>

#include "ReaderWriter.h"

ReaderWriter::ReaderWriter() {
}

void ReaderWriter::acquire_read() {
  read_lock.lock();
  if (++readers == 1) {
    write_lock.lock();
  }
  read_lock.unlock();
}

void ReaderWriter::release_read() {
  read_lock.lock();
  if (--readers == 0) {
    write_lock.unlock();
  }
  read_lock.unlock();
}

void ReaderWriter::acquire_write() {
  write_lock.lock();
}

void ReaderWriter::release_write() {
  write_lock.unlock();
}
