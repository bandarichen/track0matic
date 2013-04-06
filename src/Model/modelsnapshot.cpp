#include "modelsnapshot.h"

namespace Model
{

Snapshot::Snapshot(std::shared_ptr<std::set<std::shared_ptr<Track> > > data)
  : data_(data)
{}

std::shared_ptr<
                std::set<std::shared_ptr<Track> >
               > Snapshot::getData() const
{
  return data_;
}

} // namespace Model
