#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QWindow>
#include <QTimer>
#include <QDesktopWidget>

#include <sstream>
#include <iostream>
#include <chrono>
#include <unistd.h>

using csc = std::chrono::steady_clock;

struct Line
{
  int x, y, s;
};

FILE* file;
int fd;
char buffer[32768];
int bufferPos;
csc::time_point lastWindowRead = csc::now();

QMainWindow* mw;
int xspeed = 5;
int xdir = 1;
int fallSpeed = 4;


int wwidth;
int wheight;
std::vector<QRect> windows;
std::vector<Line> hLines;
std::vector<Line> vLines;

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
void filterH(Line& line, QRect const& mask)
{
  if (line.y < mask.top() || line.y > mask.bottom())
    return;
  if (line.x > mask.right() || line.x+line.s < mask.left())
    return;
  if (line.x > mask.left())
  {
    int delta = mask.right()-line.x;
    line.x += delta;
    line.s -= delta;
  }
  if (line.x+line.s < mask.right())
  {
    int delta = mask.left()-line.x-line.s;
    line.s -= delta;
  }
  // FIXME: doesn't deal with center cut
}

void filterV(Line& line, QRect const& mask)
{
  if (line.x < mask.left() || line.x > mask.right())
    return;
  if (line.y > mask.bottom() || line.y+line.s < mask.top())
    return;
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
  // FIXME: doesn't deal with center cut
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
    for (int j=0; j<i; j++)
    {
      filterH(top, windows[j]);
      filterH(bottom, windows[j]);
      filterV(left, windows[j]);
      filterV(right, windows[j]);
    }
    if (top.s > 0)
      hLines.push_back(top);
    if (bottom.s > 0)
      hLines.push_back(bottom);
    if (left.s > 0)
      vLines.push_back(left);
    if (right.s > 0)
      vLines.push_back(right);
  }
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
void fire()
{
  auto r = mw->geometry();
  auto bcx = r.left()+r.width()/2;
  auto bcy = r.bottom();
  int tgtx = r.left();
  int tgty = r.top();
  if (xdir > 0 && r.right() >= wwidth - 3)
    xdir = -1;
  if (xdir < 0 && r.left() <= 3)
    xdir = 1;
  if (hasGround(bcx+xspeed*xdir, bcy))
  {
    tgtx += xspeed * xdir;
  }
  else if (hasGround(bcx, bcy))
  {
    xdir *= -1;
    tgtx += xspeed * xdir;
  }
  else
  {
    tgty += fallSpeed;
  }
  mw->windowHandle()->setPosition(tgtx, tgty);

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
  QTimer::singleShot(std::chrono::milliseconds(10), &fire);
}

int main(int argc, char **argv)
{
  QApplication app(argc, argv);
     auto const rec = QApplication::desktop()->screenGeometry();
   wheight = rec.height();
   wwidth = rec.width();
  QPixmap pix("./critter.png");
  
  mw = new QMainWindow();
  mw->setStyleSheet("background:transparent");
  mw->setAttribute(Qt::WA_TranslucentBackground);
  mw->setWindowFlags(Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
  auto* l = new QLabel();
  l->setPixmap(pix);
  mw->setCentralWidget(l);
  mw->show();
  mw->windowHandle()->setPosition(10,10);
  QTimer::singleShot(std::chrono::milliseconds(10), &fire);
  app.exec();
}