#include <jni.h>

JNIEXPORT jstring JNICALL
Java_com_example_atomic_1data_1test_atomic_1data_1test_atomicDataTest( JNIEnv *env, jclass type ) {

  char *atomic_data_log();
  int atomic_data_test();

  atomic_data_test();

  return ( *env )->NewStringUTF( env, atomic_data_log());

}