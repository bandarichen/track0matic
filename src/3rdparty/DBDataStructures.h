#ifndef DBDATASTRUCTURES_H
#define DBDATASTRUCTURES_H
#include "Exceptions.h"
#include "ConstrainedNumeric.hpp"
#include <pqxx/pqxx>

#include <set>
#include <vector>
#include <memory>

namespace Model
{

typedef ConstrainedNumeric<-180, 180> Longitude;
typedef ConstrainedNumeric<-90, 90> Latitude;
typedef ConstrainedNumeric<-420, 8850> MetersOverSea;

struct StreetNode
{
  StreetNode(pqxx::tuple tableRow);

  int nodeId;
  Longitude lon;
  Latitude lat;
  MetersOverSea mos;
};

typedef std::shared_ptr<StreetNode> StreetNodePtr;
typedef std::vector<StreetNodePtr> StreetNodes;

struct Street
{
  Street(pqxx::tuple tableRow,
         const StreetNodes& vertexes); //TODO use std::set here

  StreetNodePtr first;
  StreetNodePtr second;
};

typedef std::shared_ptr<Street> StreetPtr;
typedef std::vector<StreetPtr> Streets;

struct Map
{
  Map(const StreetNodes& vertexes, const Streets& edges);
  Streets streetsInVertex(StreetNodePtr vertex);

  StreetNodes vertexes;
  Streets     edges;
  double      normalizationVector[2];
};

typedef std::shared_ptr<Map> MapPtr;

}//namespace Model

#endif // DBDATASTRUCTURES_H
