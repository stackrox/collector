//
// Created by Malte Isberner on 9/12/18.
//

#include <fcntl.h>
#include "FileSystem.h"

namespace collector {

int Foo() {
  FDHandle fd = open("foo", O_RDONLY);

  FDHandle fd2 = open("bar", O_RDONLY);
  if (fd < fd2) {
    return 1;
  }
  return 0;
}

}