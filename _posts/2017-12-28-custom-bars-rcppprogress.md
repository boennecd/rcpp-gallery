---
title: Custom progress bars for RcppProgress
author: Clemens Schmid and Karl Forner
license: GPL (>= 2)
tags: openmp
summary: Demonstrates how to implement custom progress bars for long computations in C++.
layout: post
src: 2017-12-28-custom-bars-rcppprogress.Rmd
---

[RcppProgress](http://cran.r-project.org/web/packages/RcppProgress/index.html) 
is a tool to help you monitor the execution time of your C++ code, by
providing a way to interrupt the execution inside the C++ code, and also to
display a progress bar indicative of the state of your computation. Additionally, 
it is compatible with multi-threaded code, for example using OpenMP. 
[The initial (yet updated) article](https://gallery.rcpp.org/articles/using-rcppprogress/) explains the 
basic setup.

Since version 0.4 it became more simple to create custom progress bars. In this new
article we will show how to do this. Our final example displays a progress bar which 
provides an estimation of the remaining time (ETA) to finish a computation.

### A minimal example

Imagine you added a progress bar with RcppProgress to your function
`long_computation()` following the example from the first article mentioned above.


{% highlight cpp %}
// [[Rcpp::depends(RcppProgress)]]
#include <progress.hpp>
#include <progress_bar.hpp>
// [[Rcpp::export]]
double long_computation(int nb, bool display_progress=true) {
    double sum = 0;
    Progress p(nb*nb, display_progress);
    for (int i = 0; i < nb; ++i) {
        if (Progress::check_abort() )
            return -1.0;
        for (int j = 0; j < nb; ++j) {
            p.increment(); 
            sum += R::dlnorm(i+j, 0.0, 1.0, 0);
        }
    }
    return sum + nb;
}
{% endhighlight %}


{% highlight r %}
long_computation(10)
{% endhighlight %}



<pre class="output">
[1] 12.2038
</pre>

What you get is a basic and useful console visualization that looks like this: 

```
0%   10   20   30   40   50   60   70   80   90   100%
[----|----|----|----|----|----|----|----|----|----|
******************************
```

That's the default, platform independent display in RcppProgress defined in 
[SimpleProgressBar.hpp](https://github.com/kforner/rcpp_progress/blob/master/inst/include/simple_progress_bar.hpp). 
It's OK for most purposes to give you an idea how much work is done and it also allows
you to make a very intuitive estimation about how long it's going to take to finish.
But of course that's not everything a progress bar *could* show you. A progress bar 
could give you information about the running progress or about performance parameters 
of your system. It could contain calculated estimates of passed and remaining time. 
After all it could just look much more fancy to impress your colleagues.

RcppProgress makes it now easy to create your own implementation of a progress bar class. 
Your own class has to be derived from the abstract class `ProgressBar` that defines some 
basic virtual methods:


{% highlight cpp %}
class ProgressBar {
  public:
    virtual ~ProgressBar() = 0;
    virtual void display() = 0;
    virtual void update(float progress) = 0;
    virtual void end_display() = 0;
};
{% endhighlight %}

`display()` starts the display that will be updated by subsequent calls of
`update()`. `end_display` finalizes it. Your progress bar implementation should
not rely on the destructor to finalize the display.

A very **minimal setup** could look something like this: 


{% highlight cpp %}
#include <R_ext/Print.h>

// [[Rcpp::depends(RcppProgress)]]
#include <progress.hpp>
#include "progress_bar.hpp"

class MinimalProgressBar: public ProgressBar{
  public:
    MinimalProgressBar()  {
        _finalized = false;
    }
    
    ~MinimalProgressBar() {}
    
    void display() {
      REprintf("Progress: ");
    }
    
    void update(float progress) {
      if (_finalized) return;
      REprintf("+");
    }
    
    void end_display() {
      if (_finalized) return;
      REprintf("\n");
      _finalized = true;
    }
    
  private:
  
    bool _finalized;
          
};

// [[Rcpp::export]]
double long_computation_minimal(int nb, bool display_progress=true) {
    MinimalProgressBar pb;
    double sum = 0;
    Progress p(nb, display_progress, pb);
    for (int i = 0; i < nb; ++i) {
        if (Progress::check_abort() )
            return -1.0;
        for (int j = 0; j < nb; ++j) {
            sum += R::dlnorm(i+j, 0.0, 1.0, 0);
        }
        p.increment(); 
    }
    return sum + nb;
}
{% endhighlight %}


{% highlight r %}
long_computation_minimal(10)
{% endhighlight %}



<pre class="output">
[1] 12.2038
</pre>

The `display()` method in this example does nothing more than printing the word 
`Progress`. `update()` concatenates a `+` symbol every time `Progress::increment()` is 
called. The result looks like this:

```
Progress: ++++++++++
```

In comparison to the example of the default progress bar above, I moved the 
call to `increment()` out of the second level and into the first level loop to keep
the amount of console output at bay. `update()` also checks if the display is `_finalized`.
`end_display` triggers the finalization.  

### Remaining time estimation

Based on the minimal setup above, you can implement more sophisticated 
progress bars. Here's an example of one that looks exactly like the default 
`SimpleProgressBar`, but adds an estimation of the remaining time for the process to finish. 
You can find a complete package setup with the code for this ETAProgressBar 
[here](https://github.com/kforner/rcpp_progress/tree/master/inst/examples/RcppProgressETA). 
In this article we only highlight some crucial aspects of the implementation.  

We use the [Rinterface.h](https://stat.ethz.ch/R-manual/R-devel/RHOME/doc/manual/R-exts.html#Setting-R-callbacks) header to update the display dynamically. Unfortunately this [header is only available for Unix-like systems](https://stackoverflow.com/questions/47623478/creating-a-progress-update-by-replacing-output-in-the-r-console-from-c-c/47624175?noredirect=1#comment82228757_47624175). 
A less cool, old version of an ETA progress bar that also works on windows can be 
found [here](https://github.com/kforner/rcpp_progress/blob/5b0ec0d672c7758cf4c4134e97dfa9921ac668e0/inst/examples/RcppProgressETA/src/eta_progress_bar.hpp). 
The following preprocessor statements load Rinterface.h if the code is compiled 
on a non-windows computer.  


{% highlight cpp %}
#if !defined(WIN32) && !defined(__WIN32) && !defined(__WIN32__)
#include <Rinterface.h>
#endif
{% endhighlight %}

The class `ETAProgressBar` inherits from the abstract class `ProgressBar`. 
It has an integer variable `_max_ticks` that controls the amount of individual
tick symbols necessary to reach the 100% mark of the progress bar. That depends
on the display you want to craft. `ETAProgressBar` also has a boolean flag variable 
`_timer_flag` that acts as a switch to separate the initial starting turn where 
the time measurement starts and the following turns where the time is picked off.
The measured time values are stored in two variables `start` and `end` of class
`time_t` (from [ctime](http://www.cplusplus.com/reference/ctime/)).


{% highlight cpp %}
class ETAProgressBar: public ProgressBar{

  // ...
  
  private: 
    int _max_ticks;
    bool _finalized;
    bool _timer_flag;
    time_t start,end;
    
  // ...  
    
}
{% endhighlight %}

The `display()` function initializes the progress bar visualization. The first two lines
are hard coded ASCII art.


{% highlight cpp %}
void display() {
  REprintf("0%%   10   20   30   40   50   60   70   80   90   100%%\n");
  REprintf("[----|----|----|----|----|----|----|----|----|----|\n");
  flush_console();
}
{% endhighlight %}

`update()` is the most important function for the progress bar mechanism. The if clause
allows to separate the initial call of `update()` from the following ones to start the time
counter. Afterwards the time passed is calculated and transformed to a human readable string
by the custom function `_time_to_string()`. `_current_ticks_display()` is another custom 
function to transform the progress information to a string with the correct amount of `*`
symbols and filling whitespaces. The progress string and the time string are concatenated
to create the additional third line below the initial two lines drawn by `display()`. 
A string with sufficient whitespaces is also added to ensure that this dynamically updated 
line is overwritten completely from turn to turn. `REprintf("\r");` triggers a carriage return
to make this continuous overwriting possible.


{% highlight cpp %}
void update(float progress) {
  
    // stop if already finalized
    if (_finalized) return;
  
    // start time measurement when update() is called the first time
    if (_timer_flag) {
        _timer_flag = false;
        // measure start time
        time(&start);
    } else {
    
        // measure current time
        time(&end);
    
        // calculate passed time and remaining time (in seconds)
        double pas_time = std::difftime(end, start);
        double rem_time = (pas_time / progress) * (1 - progress);
    
        // convert seconds to time string
        std::string time_string = _time_to_string(rem_time);
    
        // create progress bar string 
        std::string progress_bar_string = _current_ticks_display(progress);
    
        // ensure overwriting of old time info
        int empty_length = time_string.length();
        std::string empty_space = std::string(empty_length, ' ');
    
        // merge progress bar and time string
        std::stringstream strs;
        strs << "|" << progress_bar_string << "| " << time_string << empty_space;
        std::string temp_str = strs.str();
        char const* char_type = temp_str.c_str();
    
        // print: remove old display and replace with new
        REprintf("\r");
        REprintf("%s", char_type);
    
        // finalize display when ready
        if(progress == 1) {
           _finalize_display();
        }  
    }
}
{% endhighlight %}

`_time_to_string()` parses time information given in form of a floating point number of 
seconds to a human-readable string. The basic algorithm is based on an example from 
[programmingnotes.org](http://www.programmingnotes.org/?p=2062). 


{% highlight cpp %}
std::string _time_to_string(double seconds) {
  
    int time = (int) seconds;
  
    int hour = 0;
    int min = 0;
    int sec = 0;
  
    hour = time / 3600;
    time = time % 3600;
    min = time / 60;
    time = time % 60;
    sec = time;
  
    std::stringstream time_strs;
    if (hour != 0) time_strs << hour << "h ";
    if (min != 0) time_strs << min << "min ";
    if (sec != 0) time_strs << sec << "s ";
    std::string time_str = time_strs.str();
  
    return time_str;
}
{% endhighlight %}

`_current_ticks_display()` relies on `_compute_nb_ticks()` to first of all transform
the progress information (floating point number between 0 and 1) to a natural number
that expresses the correct fraction of `_max_ticks`. `_construct_ticks_display_string()`
takes this value and parses a string with `*` symbols and whitespaces that can be plotted
as a visual progress indication.


{% highlight cpp %}
std::string _current_ticks_display(float progress) {
    int nb_ticks = _compute_nb_ticks(progress);
    std::string cur_display = _construct_ticks_display_string(nb_ticks);
    return cur_display;
}

int _compute_nb_ticks(float progress) {
    return int(progress * _max_ticks);
}

std::string _construct_ticks_display_string(int nb) {
    std::stringstream ticks_strs;
    for (int i = 0; i < (_max_ticks - 1); ++i) {
        if (i < nb) {
            ticks_strs << "*";
        } else {
            ticks_strs << " ";
        }
    }
    std::string tick_space_string = ticks_strs.str();
    return tick_space_string;
}
{% endhighlight %}

`flush_console()` is a wrapper around [`R_FlushConsole()`](https://cran.r-project.org/doc/manuals/r-release/R-exts.html#Setting-R-callbacks) which is called to flush any 
pending output to the system console. It's necessary to do this when the display is started
in `display()` and when it's closed in `_finalize_display()`.


{% highlight cpp %}
void flush_console() {
#if !defined(WIN32) && !defined(__WIN32) && !defined(__WIN32__)
          R_FlushConsole();
#endif
}
{% endhighlight %}

The output of an `ETAProgressBar` looks like this:

```
0%   10   20   30   40   50   60   70   80   90   100%
[----|----|----|----|----|----|----|----|----|----|
|*******                                          | 49s
```
