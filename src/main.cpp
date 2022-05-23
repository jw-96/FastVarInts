#include "pybind11/pybind11.h"
#include "pybind11/numpy.h"

//#include <pybind11/pybind11.h>
//#include <pybind11/numpy.h>
#include <math.h>
#include <iostream>
#include <cstring>
#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

namespace py = pybind11;

py::array_t<u_int8_t> compress(py::array_t<u_int32_t> array){
  // todo: currently we write the bytes right to left (mostly), shuld write them left to right
  const ssize_t* sizes = array.shape();
  auto data = array.data();
  std::vector<u_int8_t>* compressed = new std::vector<u_int8_t>();
  int N = sizes[0];
  int index = 0;
  int left = 8;
  u_int8_t cur = 0;
  long total_size = 0;
  long bytes = 1;
  // py::print("integer: ", N);
  for(int i=0; i < N; i++){
    u_int32_t value = data[i];
    // py::print("value: ", value);
    // only accurate upt to 2^52, use the following for long implementation:
    // https://stackoverflow.com/questions/994593/how-to-do-an-integer-log2-in-c
    int n = (int) log2((double) value); // can be done faster?, Look at highest 1?
    int np1 = n + 1;
    // py::print("logs: ", n, np1, cur, left);
    total_size += n + np1;
    while(n > 0){
      if (left > n){
        left -= n;
        n = 0;
      }else{
        // py::print("pushed: ", cur);
        compressed->push_back(cur);
        n = n - left;
        cur = 0;
        left = 8;
        bytes++;
      }
    }
    // py::print("written logs: ", cur, left);
    while (np1 > 0){
      if (left > 0 && np1 > left){
        u_int8_t mask = 255u;
        mask = mask >> (8 - left);
        np1 = np1 - left;
        cur = cur | ((value >> np1) & mask);
        // py::print("pushed: ", cur);
        compressed->push_back(cur);
        cur = 0;
        left = 8;
        bytes++;
      }else if (left > 0 && np1 <= left){
        u_int8_t mask = 255u;
        mask = mask >> (8 - left);
        cur = cur | ((value << (left -np1)) & mask);
        // py::print("cur: ", cur);
        left -= np1;
        np1 = 0;
      }else{
        // py::print("pushed: ", cur);
        compressed->push_back(cur);
        cur = 0;
        left = 8;
        bytes++;
      }
    }
    if(left == 0){
      // py::print("pushed: ", cur);
      compressed->push_back(cur);
      cur = 0;
      left = 8;
      bytes++;
    }
  }
  if(left < 8) {
    // py::print("pushed: ", cur);
    compressed->push_back(cur);
    cur = 0;
    left = 8;
    bytes++;
  }else{
    // -1 because we will add 8 later
    bytes -= 1;
  }
  for(int i = 7; i >= 0; i--){
    cur = 0;
    cur = cur | (total_size >> i*8);
    // py::print("pushed: ", cur);
    compressed->push_back(cur);
  }
  // py::print("total size: ", total_size, "bytes: ", bytes);
  bytes += 8;

  auto capsule = py::capsule(compressed, [](void *v) {
    //std::cerr << "freeing memory @ " << v << "\n";
    delete reinterpret_cast<std::vector<u_int8_t>*>(v);
  });
  return py::array(compressed->size(), compressed->data(), capsule);
}


py::array_t<u_int32_t> decompress(py::array_t<u_int8_t> array){
  // todo: currently we write the bytes right to left (mostly), shuld write them left to right
  const ssize_t* sizes = array.shape();
  auto data = array.data();
  std::vector<u_int32_t>* uncomp = new std::vector<u_int32_t>();
  int N = sizes[0];
  int index = 0;
  long total_size = 0;
  long bytes = 1;

  u_int64_t length;;

  for(int i = 8; i > 0; i--){
    u_int64_t d = (u_int64_t) data[N-i];
    length = length | d << (i- 1)*8;
  }
  // py::print("length", length);


  u_int32_t cur_dec = 0;
  int n = 0;
  int var_len = 0;
  u_int8_t prob = 127u;
  bool phase = true;
  int i = 0;
  u_int8_t cur = data[0];
  int left = 8;
  u_int32_t l = 0;
  while (l < length){
    if (left == 0){
      i++;
      cur = data[i];
      // py::print("cur:", cur, i);
      left = 8;
    }

    u_int8_t cur_copy = cur;
    cur  = cur << (8 - left);
    while(phase && left > 0){
      if ((cur >> 7) == 0 ){
        cur = cur << 1;
        n ++;
        left --;
      }else{
        phase = false;
        l += n;
        n++;
      }
    }
    cur = cur_copy;
    // py::print("n:", n, "left:", left, "cur:", cur);

    while(!phase && left > 0){
      u_int8_t mask = 255u;
      mask = mask >> (8-left);

      u_int32_t cur32 = (u_int32_t) (cur & mask);
      if (n >= left){ // TODO, change to >, makes code easier
        n -= left;
        l += left;
        left = 0;
        cur_dec = cur_dec | (cur32 << n);
        if (n == 0){
          // py::print("at ", uncomp->size(), "have", cur_dec);
          uncomp->push_back(cur_dec);
          cur_dec = 0u;
          phase = true;
        }
      }else{
        left -= n;
        l += n;
        cur_dec = cur_dec | (cur32 >> left);
        n = 0;
        // py::print("at ", uncomp->size(), "have", cur_dec);
        uncomp->push_back(cur_dec);
        cur_dec = 0u;
        phase = true;
      }
    }
  }

  auto capsule = py::capsule(uncomp, [](void *v) {
    //std::cerr << "freeing memory @ " << v << "\n";
    delete reinterpret_cast<std::vector<u_int32_t>*>(v);
  });
  return py::array(uncomp->size(), uncomp->data(), capsule);
}


PYBIND11_MODULE(_core, m) {
    m.doc() = R"pbdoc(
        Fast Variable Length Encodings For Python
        -----------------------

        .. currentmodule:: fastvarints

        .. autosummary::
           :toctree: _generate

           compress
           decompress
    )pbdoc";


    m.def("compress", &compress, R"pbdoc(
        compresses a numpy array using the elias gamma encoding

    )pbdoc");

    m.def("decompress", &decompress, R"pbdoc(
        decompresses a numpy byte array with elias gamma encoded numbers

    )pbdoc");


#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
