
##atomic_data: A Multibyte General Purpose Lock-Free Data Structure

  This repo conrains three folders:

  * In [Samples](tree/master/Samples) you'll find **atomic\_data** samples such as
  a concurrent linked list with insertion/removal at any point, a std::vector of 
  **atomic\_data** and a **atomic\_data** of std::vector and the one, that I used
  to test the correctness - **atomic\_data** wrapping an array and threads incrementing
  the minimum value in the array. Most samples include a run using a bare std::mutex for
  performance comparison.

  * [Vistual Studio 2015](tree/master/VisualStudio2015) project with above samples.

  * [Android Studio](tree/master/AndroidStudio) NDK project to test **atomic\_data** on 
  a smartphone.

<!-- markdown bug-->

  Read more in the [blog post](http://alexpolt.github.io/atomic-data.html).

  A [diagram](http://alexpolt.github.io/images/atomic-data.png) of **atomic\_data** operation.


##License

  The code in this repository is Public-domain software.

  ![Pubic domain software](http://alexpolt.github.io/images/public_domain_mark.png)

