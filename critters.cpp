#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QWindow>
#include <QTimer>
#include <QDesktopWidget>
#include <QDir>

#include <sstream>
#include <iostream>
#include <chrono>
#include <unordered_map>
#include <unistd.h>

using csc = std::chrono::steady_clock;

struct Line
{
  int x, y, s;
};

class Anims
{
public:
  void load(std::string const& dir);
  void play(std::string const& anim, int speed=1);
  bool finished();
  void step();
  int nFrames(std::string const& anim);
private:
  std::unordered_map<std::string, std::vector<QPixmap>> frames;
  std::string current;
  int speed;
  int pos;
};
enum class State
{
  Walking,
  Howling,
  Sitting, // transition
  Seated, // idle
  Standing, // transition
  Up, // idle
  Jumping,
  Bite,
};

template<typename T>
int millis(T const& elapsed)
{
  return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
}

FILE* file;
int fd;
char buffer[32768];
int bufferPos;
csc::time_point lastWindowRead = csc::now();

QMainWindow* mw;
QLabel* label;
Anims anims;
const int xSpeedWalk = 3;
const int xSpeedRun = 6;
const int fallSpeed = 4;
const int jumpScanX = 250;
const int jumpScanY = 150;
int xspeed = 5;
int yspeed = 0;
int xdir = 1;

State state = State::Walking;
csc::time_point stateStartTime = csc::now();
csc::time_point stateEndTime;
csc::time_point lastBite;


int wwidth;
int wheight;
std::vector<QRect> windows;
std::vector<Line> hLines;
std::vector<Line> vLines;

int Anims::nFrames(std::string const& anim)
{
  return frames[anim].size();
}

void Anims::play(std::string const& anim, int speed)
{
  if (anim == current)
    return;
  current = anim;
  this->speed = speed;
  pos = 0;
}

void Anims::step()
{
  if (current == "")
    return;
  if (pos % speed)
  {
    pos++;
    return;
  }
  int fidx = pos / speed;
  auto& a = frames[current];
  label->setPixmap(a[fidx%a.size()]);
  pos++;
}

bool Anims::finished()
{
  if (current == "")
    return true;
  int fidx = pos / speed;
  auto& a = frames[current];
  return fidx >= a.size();
}

void Anims::load(std::string const& path)
{
  QDir dir(path.c_str());
  std::vector<std::string> files;
  for (const QString &filename : dir.entryList(QDir::Files))
    files.push_back(filename.toStdString());
  std::sort(files.begin(), files.end());
  for (auto const& f: files)
  {
    QPixmap pix((path + "/" + f).c_str());
    auto p = f.find_first_of('0');
    auto base = f.substr(0, p-1);
    frames[base].push_back(pix);
  }
}

bool hasGround(int x, int y)
{
  for (auto const& l: hLines)
  {
    if (l.y >= y && l.y < y+6 && l.x <= x && l.x+l.s >= x)
    {
      std::cerr << "hit " << x << " " << y << " on " << l.x << " " << l.y << " " <<l.s << std::endl;
      return true;
    }
  }
  std::cerr << "miss " << x << " " << y << std::endl;
  return false;
}

bool hasGroundIntersecting(Line const& v, int& yIntersect)
{
  for (auto const& l: hLines)
  {
    if (l.x < v.x && l.x +l.s >v.x && l.y > v.y && l.y < v.y+v.s)
    {
      yIntersect = l.y;
      return true;
    }
  }
  return false;
}

void filterH(Line& line, int begin, int end)
{
  for (int i=begin; i<end; i++)
  {
    auto const& mask = windows[i];
    if (line.y < mask.top() || line.y > mask.bottom())
      continue;
    if (line.x > mask.right() || line.x+line.s < mask.left())
      continue;
    if (line.x >= mask.left() && line.x < mask.right())
    {
      int delta = mask.right()-line.x;
      line.x += delta;
      line.s -= delta;
    }
    if (line.s > 0 && line.x+line.s < mask.right())
    {
      line.s = mask.left()-line.x;
    }
    if (line.s >0 && line.x < mask.left() && line.x+line.s > mask.right())
    {
      Line lpart{line.x, line.y, mask.left()-line.x};
      Line rpart{mask.right(), line.y, line.x+line.s-mask.right()};
      filterH(lpart, begin+1, end);
      filterH(rpart, begin+1, end);
      return;
    }
    if (line.s <= 0)
      return;
  }
  hLines.push_back(line);
}

void filterV(Line& line, int begin, int end)
{
  for (int i=begin; i<end; i++)
  {
    auto const& mask = windows[i];
    if (line.x < mask.left() || line.x > mask.right())
      continue;
    if (line.y > mask.bottom() || line.y+line.s < mask.top())
      continue;
    if (line.y > mask.top())
    {
      int delta = mask.bottom()-line.y;
      line.y += delta;
      line.s -= delta;
    }
    if (line.y+line.s < mask.bottom())
    {
      int delta = mask.top()-line.y-line.s;
      line.s -= delta;
    }
    if (line.s >0 && line.y < mask.top() && line.y+line.s > mask.bottom())
    {
      Line lpart{line.x, line.y, mask.top()-line.y};
      Line rpart{line.x, mask.bottom(), line.y+line.s-mask.bottom()};
      filterV(lpart, begin+1, end);
      filterV(rpart, begin+1, end);
      return;
    }
    if (line.s <= 0)
      return;
  }
  vLines.push_back(line);
}
void fuseHLines()
{
  for (int a=0; a < hLines.size(); a++)
  {
    for (int b=a+1; b < hLines.size();b++)
    {
      auto& la = hLines[a];
      auto& lb = hLines[b];
      if (la.y == lb.y)
      {
        if (la.x <= lb.x && la.x+la.s >= lb.x)
        {
          la.s = std::max(la.s, lb.x+lb.s-la.x);
          std::swap(hLines[b], hLines[hLines.size()-1]);
          hLines.pop_back();
          b--;
        }
        else if (lb.x <= la.x && lb.x+lb.s >= la.x)
        {
          int end = std::max(la.x+la.s, lb.x+lb.s);
          la.x = lb.x;
          la.s = end - la.x;
          std::swap(hLines[b], hLines[hLines.size()-1]);
          hLines.pop_back();
          b--;
        }
      }
    }
  }
}
void windowsToLines()
{
  hLines.clear();
  vLines.clear();
  for (int i=0; i< windows.size(); i++)
  {
    auto& cand = windows[i];
    Line top = Line{cand.left(), cand.top(), cand.width()};
    Line bottom = Line{cand.left(), cand.bottom(), cand.width()};
    Line left = Line{cand.left(), cand.top(), cand.height()};
    Line right = Line{cand.right(), cand.top(), cand.height()};
    filterH(top, 0, i);
    filterH(bottom, 0, i);
    filterV(left, 0, i);
    filterV(right, 0, i);
  }
  hLines.push_back(Line{0,wheight, wwidth});
  fuseHLines();
  // debug
  for (auto const& l: hLines)
  {
    std::cerr << l.x << " " << l.y << " " << l.s << std::endl;
  }
}
void startReadWindows()
{
  file = popen("./tlw.sh", "r");
  fd = fileno(file);
  bufferPos = 0;
}

std::vector<QRect> parseWindows(std::string const& data)
{
  std::stringstream ss(data);
  std::vector<QRect> res;
  while (ss.good())
  {
    std::string entry;
    ss >> entry;
    if (entry.length()==0)
      continue;
    auto x = entry.find_first_of('x');
    auto plus1 = entry.find_first_of('+');
    auto plus2 = entry.find_first_of('+', plus1+1);
    int w = std::stoi(entry.substr(0, plus1));
    int h = std::stoi(entry.substr(x+1, plus1-x-1));
    int l = std::stoi(entry.substr(plus1+1, plus2-plus1-1));
    int t = std::stoi(entry.substr(plus2+1));
    //std::cerr << entry << " " << w << " " << h << " " << l << " " << t  << std::endl;
    res.emplace_back(l, t, w, h);
    std::cerr << entry << " => " << res.back().left() << " " << res.back().top() << " " << res.back().width() << " " << res.back().height() << std::endl;
  }
  return res;
}


bool tryReadWindows(std::vector<QRect> & wins)
{
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(fd, &rfds);
  timeval tv = {0,0};
  int retval = select(fd+1, &rfds, nullptr, &rfds, &tv);
  if (retval == -1)
  {
    perror("select()");
    return false;
  }
  if (retval)
  {
    int cnt = read(fd, buffer+bufferPos, 32768-bufferPos);
    if (cnt > 0)
      bufferPos += cnt;
    else
    {
      wins = parseWindows(std::string(buffer, bufferPos));
      fclose(file);
      file = nullptr;
      fd = -1;
      lastWindowRead = csc::now();
      return true;
    }
  }
  return false;
}
void walkOrRun()
{
  bool run = rand()%2;
  xspeed = run ? xSpeedRun : xSpeedWalk;
  if (run)
    anims.play((xdir > 0)?"run_r":"run", 3);
  else
    anims.play((xdir>0)?"walk_r":"walk", 3);
}
void fire()
{
  auto r = mw->geometry();
  auto bcx = r.left()+r.width()/2;
  auto bcy = r.bottom();
  int tgtx = r.left();
  int tgty = r.top();
  if (state == State::Sitting && anims.finished())
  {
    state = State::Seated;
    stateStartTime = csc::now();
    stateEndTime = stateStartTime + std::chrono::milliseconds(2000 + rand()%6000);
    anims.play((xdir > 0)?"sitting_r":"sitting", 6);
  }
  if (state == State::Standing && anims.finished())
  {
    state = State::Walking;
    stateStartTime = csc::now();
    walkOrRun();
  }
  if (state == State::Seated && stateEndTime <= csc::now())
  {
    state = State::Standing;
    anims.play((xdir > 0)?"stand_r":"stand", 6);
    stateStartTime = csc::now();
  }
  if (state == State::Up && stateEndTime <= csc::now())
  {
    state = State::Walking;
    stateStartTime = csc::now();
    walkOrRun();
  }
  if (state == State::Howling && anims.finished())
  {
    state = State::Walking;
    stateStartTime = csc::now();
    walkOrRun();
  }
  if (state == State::Walking)
  {
    if (millis(csc::now() - stateStartTime) > 3000)
    {
      if (rand()%500 == 0)
      {
        state = State::Howling;
        anims.play((xdir > 0)?"howl_r":"howl", 8);
      }
      else if (rand()%500 == 0)
      {
        state = State::Sitting;
        anims.play((xdir > 0)?"sit_r":"sit", 8);
      }
      else if (rand()%500 == 0)
      {
        state = State::Up;
        stateStartTime = csc::now();
        stateEndTime = stateStartTime + std::chrono::milliseconds(2000 + rand()%8000);
        anims.play((xdir > 0)?"idle_r":"idle", 7);
      }
    }
  } 
  if (state == State::Bite && anims.finished())
  {
    state = State::Walking;
    walkOrRun();
  }
  if (state == State::Walking || state == State::Up)
  {
    QPoint globalCursorPos = QCursor::pos();
    QPoint mouthPos = QPoint(bcx + r.width()*1/4*xdir, bcy-r.height()*4/7);
    if ((mouthPos-globalCursorPos).manhattanLength() < 25 && csc::now()-lastBite > std::chrono::milliseconds(1000))
    {
      state = State::Bite;
      anims.play((xdir > 0)?"bite_r":"bite", 7);
      lastBite = csc::now();
    }
  }
  if (state == State::Walking)
  {
    if (xdir > 0 && r.right() >= wwidth - 3)
    {
      xdir = -1;
      walkOrRun();
    }
    if (xdir < 0 && r.left() <= 3)
    {
      xdir = 1;
      walkOrRun();
    }
    if (hasGround(bcx+xspeed*xdir, bcy))
    {
      // sometimes jump up if possible
      bool tryJump = true; // (rand()%4) == 0;
      if (tryJump)
      {
        int ty;
        if (hasGroundIntersecting(Line{bcx + jumpScanX*xdir, bcy-jumpScanY, jumpScanY-8}, ty))
        { // jump
          state = State::Jumping;
          int naf = anims.nFrames("jump");
          int jt = jumpScanX / xSpeedRun;
          anims.play((xdir > 0)?"jump_r":"jump", jt / naf);
          xspeed = xSpeedRun * xdir;
          yspeed = -20;
        }
      }
      if (state == State::Walking)
        tgtx += xspeed * xdir;
    }
    else if (hasGround(bcx, bcy)) // ledge
    {
      bool tryJump = true; // (rand()%4) == 0;
      if (tryJump)
      {
        int ty;
        if (hasGroundIntersecting(Line{bcx + jumpScanX*xdir, bcy-jumpScanY, jumpScanY*2}, ty))
        { // jump
          state = State::Jumping;
          int naf = anims.nFrames("jump");
          int jt = jumpScanX / xSpeedRun;
          anims.play((xdir > 0)?"jump_r":"jump", jt / naf);
          xspeed = xSpeedRun * xdir;
          yspeed = -20;
        }
      }
      if (state == State::Walking)
      {
        xdir *= -1;
        walkOrRun();
        tgtx += xspeed * xdir;
      }
    }
    else
    {
      tgty += fallSpeed;
    }
    if (state == State::Walking)
      mw->windowHandle()->setPosition(tgtx, tgty);
  }
  if (state == State::Jumping)
  {
    if (yspeed >=0 && hasGround(bcx, bcy))
    {
      anims.play((xdir > 0)?"idle_r":"idle", 7);
      state = State::Up;
      stateEndTime = csc::now() + std::chrono::milliseconds(800);
    }
    else
    {
      tgtx += xspeed;
      tgty += yspeed;
      yspeed++;
      if (yspeed > fallSpeed)
        yspeed = fallSpeed;
      mw->windowHandle()->setPosition(tgtx, tgty);
    }
  }
  if (file != nullptr)
  {
    bool ok = tryReadWindows(windows);
    if (ok)
    {
      windowsToLines();
      std::cerr << "got " << windows.size() << " windows" << std::endl;
    }
  }
  else if (csc::now() - lastWindowRead > std::chrono::milliseconds(300))
    startReadWindows();
  anims.step();
  QTimer::singleShot(std::chrono::milliseconds(10), &fire);
}

int main(int argc, char **argv)
{
  srand(time(nullptr));
  QApplication app(argc, argv);
  anims.load(argv[1]);
  anims.play("idle", 2);
  auto const rec = QApplication::desktop()->screenGeometry();
  wheight = rec.height();
  wwidth = rec.width();

  mw = new QMainWindow();
  mw->setStyleSheet("background:transparent");
  mw->setAttribute(Qt::WA_TranslucentBackground);
  mw->setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
  auto* l = new QLabel();
  label = l;
  mw->setCentralWidget(l);
  mw->show();
  mw->windowHandle()->setPosition(10,10);
  QTimer::singleShot(std::chrono::milliseconds(10), &fire);
  app.exec();
}