from distutils.core import setup

setup(name = 'simpleon',
      version ='0.2.0',
      description = "SimpleON (Simple Object Notation) format decoder",
      author = "Xinhao Yuan",
      author_email = "xinhaoyuan@gmail.com",
      license = "MIT",
      packages = ['simpleon' ],
      package_dir = { 'simpleon' : 'simpleon-py' }
      )
