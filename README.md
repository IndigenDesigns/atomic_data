
##atomic_data: A Multibyte General Purpose Lock-Free Data Structure

  This repo contains three folders:

  * In [Samples](Samples/) you'll find **atomic\_data** samples such as
  a concurrent linked list with insertion/removal at any point, a std::vector of 
  **atomic\_data** and a **atomic\_data** of std::vector and the one, that was
  helpful in testing memory barriers on ARM - **atomic\_data** wrapping an array 
  and threads incrementing the minimum value in the array. 
  Most samples also make a run using a bare std::mutex (*atomic\_data\_mutex*) for 
  performance comparison.

  * [Vistual Studio 2015](VisualStudio2015/) project with above samples.

  * [Android Studio](AndroidStudio/) NDK project to test **atomic\_data** on 
  a smartphone.


###A diagram depicting the basics of the algorithm behind **atomic\_data**:

  ![diagram](http://alexpolt.github.io/images/atomic-data.png)

  Read more in the [blog post](http://alexpolt.github.io/atomic-data.html). 


##License

  The code in this repository is Public-domain software.

  ![Pubic domain software](http://alexpolt.github.io/images/public_domain_mark.png)

