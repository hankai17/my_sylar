#include <stdio.h>
#include <time.h>
#include <iostream>
#include <list>

int list_insert()
{
    std::list<int> mylist;
    std::list<int>::iterator it1,it2;
    for (int i=1; i<10; ++i) mylist.push_back(i*10);

    for (it1=mylist.begin(); it1!=mylist.end();) {
      if(*it1 == 50) {
        it2 = mylist.insert(it1++, 0);
        std::cout << "list insert return: " << *it2 << std::endl;
      } else {
        ++it1;
      }
    }

    std::cout << "mylist contains:";
    for (it1=mylist.begin(); it1!=mylist.end(); ++it1)
      std::cout << ' ' << *it1;
    std::cout << '\n';
    return 0;
}

int list_erase()
{
    std::list<int> mylist;
    std::list<int>::iterator it1,it2;
    for (int i=1; i<10; ++i) mylist.push_back(i*10);

    for (it1=mylist.begin(); it1!=mylist.end();) {
      if(*it1 == 50) {
        it2 = mylist.erase(it1++);
        std::cout << "list erase return: " << *it2 << std::endl;
      } else {
        ++it1;
      }
    }

    std::cout << "mylist contains:";
    for (it1=mylist.begin(); it1!=mylist.end(); ++it1)
      std::cout << ' ' << *it1;
    std::cout << '\n';
    return 0;
}

int list_erase_r()
{
  std::list<int> mylist;
  std::list<int>::iterator it1,it2;
  std::list<int>::reverse_iterator it3;
  for (int i=1; i<10; ++i) mylist.push_back(i*10);

  for (it3=mylist.rbegin(); it3!=mylist.rend();) {
    if(*it3 == 50) {
      /*
      std::list<int>::reverse_iterator it4 = it3;
      it4++;
      if (it4 == mylist.rend()) {
        mylist.pop_front();
        std::cout << "erase first" << std::endl;
      } 
        else 
      */
      {
        if (0) {
            mylist.erase((++it3).base()); // *it3 == 50; *(++it3).base() == 50;
        } else {
            it2 = mylist.erase((++it3).base());
            it3 = std::list<int>::reverse_iterator(it2);
            std::cout << "erase return: it3: " << *it3 << std::endl;
            if (it3 == mylist.rend()) {
                std::cout << "it3 is rend" << std::endl;
            }
        }
      }
    } else {
      ++it3;
    }
  }

  std::cout << "mylist contains:";
  for (it1=mylist.begin(); it1!=mylist.end(); ++it1)
    std::cout << ' ' << *it1;
  std::cout << '\n';
  return 0;
}

int list_insert_r()
{
    std::list<int> mylist;
    std::list<int>::iterator it1,it2;
    std::list<int>::reverse_iterator it3;
    for (int i=1; i<10; ++i) mylist.push_back(i*10);

    for (it3=mylist.rbegin(); it3!=mylist.rend();) {
      if(*it3 == 50) {
        if (0) {
            mylist.insert((++it3).base(), 0); // 正位的50处insert
        } else {
            it2 = mylist.insert((++it3).base(), 0);
            it3 = std::list<int>::reverse_iterator(it2);
        }
      } else {
        ++it3;
      }
    }

    std::cout << "mylist contains:";
    for (it1=mylist.begin(); it1!=mylist.end(); ++it1)
      std::cout << ' ' << *it1;
    std::cout << '\n';
    return 0;
}

int list_insert_r_after()
{
    std::list<int> mylist;
    std::list<int>::iterator it1,it2;
    std::list<int>::reverse_iterator it3;
    for (int i=1; i<10; ++i) mylist.push_back(i*10);

    for (it3=mylist.rbegin(); it3!=mylist.rend();) {
      if(*it3 == 10) {
        if (0) {
            mylist.insert((it3++).base(), 0); // 正位的50后处insert
        } else {
            it2 = mylist.insert((it3++).base(), 0);
            it3 = std::list<int>::reverse_iterator(it2);
            it3++;
            std::cout << "it3: " << *it3 << std::endl;
        }
      } else {
        ++it3;
      }
    }

    std::cout << "mylist contains:";
    for (it1=mylist.begin(); it1!=mylist.end(); ++it1)
      std::cout << ' ' << *it1;
    std::cout << '\n';
    return 0;
}

int main()
{
    list_insert();
    list_erase();
    list_erase_r();
    list_insert_r();
    list_insert_r_after();
    
    /*
    printf("<<: %d\n", 1 << 0);
    printf("<<: %d\n", 1 << 1);
    printf("<<: %d\n", 1 << 2);
    double diff = 0.0;
    diff = 1.0;
    time_t now = 1634874299;

    double resu = diff + now;
    printf("resut: %d\n", resu);
    printf("resut: %f\n", resu);
    */
    return 0;
}
