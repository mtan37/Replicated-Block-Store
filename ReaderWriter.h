#ifndef __READER_WRITER_H
#define __READER_WRITER_H

//currently read-prefering
class ReaderWriter {
private:
  int readers = 0;
  std::mutex read_lock;
  std::mutex write_lock;

public:
  ReaderWriter();
  void acquire_read();
  void release_read();
  void acquire_write();
  void release_write();
};



#endif
