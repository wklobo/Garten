#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>

int main(void) {
  int i2c = open("/dev/i2c-1", O_RDWR);
  i2c_smbus_write_byte(i2c, 1);
  close(i2c);
  return 0;
}