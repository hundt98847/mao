MAO uses an instance of the free tool [Rietveld](http://codereview.appspot.com) for its code-review, deployed at http://maoreview.appspot.com

Information about the review tool can be found [here](http://code.google.com/p/rietveld/wiki/CodeReviewHelp), but here are some basic instructions to get started.

## Basic instructions ##

To request a code reivew, just run [upload.py](http://maoreview.appspot.com/static/upload.py) from your svn directory. Use the --server option to specify maoreview.appspot.com. You will also be prompted for username and password the first time you use it. You can use the same account that is used for http://code.google.com/p/mao

You can either use the web interface or options to upload.py to select reviewers and set cc addresses. Please use mao-project@googlegroups.com as a cc for all MAO issues.

Once the reviewers are happy (usually by writing LGTM), you can close the issue and submit the patch from your svn repository.

For example, to submit a single file for review (e.g., MaoScheduler.cc), you would issue a command like the following (Note that Martin, Easwaran, and Robert are the main maintainers at Google):
```
   ./upload.py -s maoreview.appspot.com -e your-email-login \
       -r martint@google.com,eraman@google.com,rhundt@google.com \
       --cc mao-project@googlegroups.com \
       --send_mail \
       src/MaoScheduler.cc
```


Additionally, please take a look at [at the main help site](http://code.google.com/p/rietveld/wiki/CodeReviewHelp) and try it out!