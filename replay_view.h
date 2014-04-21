#ifndef _REPLAY_VIEW
#define _REPLAY_VIEW

template <class T>
class ReplayView : public T {
  public:
    ReplayView(ifstream& iff) : input(iff) {
    }

    virtual void close() override {
      input.close();
      T::close();
    }

    virtual void drawLevelMap(const CreatureView*) override {
      return;
    }

    bool equal(double a, double b) {
      return fabs(a - b) < 0.001;
    }

    virtual int getTimeMilli() override {
      string method;
      int time;
      input >> method >> time;
      CHECKEQ(method, "getTime");
      return time;
    }

    virtual void presentText(const string& title, const string& text) override {
      return;
    }

    virtual bool isClockStopped() override {
      T::isClockStopped();
      string method;
      bool ret;
      input >> method >> ret;
      CHECKEQ(method, "isClockStopped");
      return ret;
    }

    virtual UserInput getAction() override {
 //     T::getAction();
 //     usleep(300000);
      string method;
      UserInput action;
      input >> method >> action;
      if (method == "exit")
        exit(0);
      CHECKEQ(method, "getAction");
      return action;
    }

    virtual Optional<int> chooseFromList(const string& title, const vector<View::ListElem>& options, int index,
        View::MenuType, double* scrollPos, Optional<UserInput::Type> a) override {
      string method;
      string action;
      input >> method >> action;
      CHECKEQ(method, "chooseFromList");
      if (action == "nothing")
        return Nothing();
      else
        return convertFromString<int>(action);
    }

    virtual Optional<Vec2> chooseDirection(const string& message) override {
      string method;
      string action;
      input >> method >> action;
      CHECKEQ(method, "chooseDirection");
      if (action == "nothing")
        return Nothing();
      else {
        vector<string> s = split(action, ',');
        CHECKEQ((int)s.size(), 2);
        return Vec2(convertFromString<int>(s[0]), convertFromString<int>(s[1]));
      }
    }

    virtual bool yesOrNoPrompt(const string& message) override {
      string method;
      bool action;
      input >> method >> action;
      CHECKEQ(method, "yesOrNoPrompt");
      return action;
    }

    virtual Optional<int> getNumber(const string& title, int min, int max, int increments) override {
      string method;
      string action;
      input >> method >> action;
      CHECKEQ(method, "getNumber");
      if (action == "nothing")
        return Nothing();
      else
        return convertFromString<int>(action);
    }
  private:
    ifstream& input;
};

#endif
