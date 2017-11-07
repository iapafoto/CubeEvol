#ifndef CREATURE_H
#define CREATURE_H

#include "include/Voxelyze.h"
#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "include/VX_Material.h"
#include "MeshRender.h"

class Creature
{
public:
	Creature();
	Creature(std::string path);
	
	~Creature();
	void clearMemory();

	CVoxelyze* voxelEngine;
	MeshRender* meshRender = NULL;

	void resetTime();
	double simulate(int id, float duration);
	float animate(int* nbTry1, int* nbTry3);

	Creature* clone();
	Creature* mutate();
	Creature* mutate(int nb);
	Creature* divide();

	bool saveJSON(std::string path);

	bool isSame(Creature* c);
	

	static bool sorter(const Creature *l, const Creature *r);

	Creature* getParent() { return parent; }

	void glInit();
	void glUpdate(GLint attribVert, GLint attribUV, GLint attribColor);
	void glRender();

	std::string getKey();

	std::string getName() { return name; }
	void setName(std::string n) { name = n;	}
	std::string getGenealogie();

	double getScore() { return score; }
	void setScore(double sc) { score = sc; }
	glm::vec3 getPosition();

private:
	double time = 0;
	double score;
	std::string key;
	std::string name="";

	Creature* parent = NULL;

	Creature(CVoxelyze* vx);
	Creature* flipMutation(int ix, int iy, int iz, int fx = 1, int fy = 1, int fz = 1);
	CVoxelyze* createCreature();
	Creature* duplicateLayerMutation();

};

#endif