/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "abc.h"
#include "frame.h"
#include "tags.h"
#include <list>
#include <time.h>
#include "swf.h"
#include "compat.h"
#include <GL/glew.h>

using namespace std;
using namespace lightspark;

long lightspark::timeDiff(timespec& s, timespec& d);

extern TLSDATA SystemState* sys;

void Frame::runScript()
{
	if(script)
		sys->currentVm->addEvent(NULL,new FunctionEvent(script));
}

void Frame::Render()
{
#ifndef WIN32
	timespec ts,td;
	clock_gettime(CLOCK_REALTIME,&ts);
#endif
	list <pair<PlaceInfo, IDisplayListElem*> >::iterator i=displayList.begin();

	//Render objects of this frame;
	for(;i!=displayList.end();i++)
	{
		assert(i->second);

		//Sync the original matrix with the current one if needed
		if(i->second->origMatrix)
			*(i->second->origMatrix)=i->second->Matrix;
		//Assign object data from current transformation
		i->second->Matrix=i->first.Matrix;
		i->second->origMatrix=&(i->first.Matrix);

		//Apply local transformation
		float matrix[16];
		i->first.Matrix.get4DMatrix(matrix);
		glPushMatrix();
		glMultMatrixf(matrix);
		i->second->Render();
		glPopMatrix();
	}
	//Render objects added at runtime
	//TODO: Add synchronization mutex, as this may be modified in the meantime by the VM
	list<IDisplayListElem*>::iterator j=dynamicDisplayList->begin();
	for(;j!=dynamicDisplayList->end();j++)
		(*j)->Render();

#ifndef WIN32
	clock_gettime(CLOCK_REALTIME,&td);
	sys->fps_prof->render_time+=timeDiff(ts,td);
#endif
}

void dumpDisplayList(list<IDisplayListElem*>& l)
{
	list<IDisplayListElem*>::iterator it=l.begin();
	for(;it!=l.end();it++)
	{
		cout << *it << endl;
	}
}

void Frame::setLabel(STRING l)
{
	Label=l;
}

void Frame::init(MovieClip* parent, list <pair<PlaceInfo, IDisplayListElem*> >& d)
{
	if(!initialized)
	{
		//Execute control tags for this frame
		//Only the root movie clip can have control tags
		if(!controls.empty())
		{
			assert(parent->root==parent);
			for(unsigned int i=0;i<controls.size();i++)
				controls[i]->execute(parent->root);
			controls.clear();

			if(sys->currentVm)
			{
				//We stop execution until execution engine catches up
				SynchronizationEvent* s=new SynchronizationEvent;
				sys->currentVm->addEvent(NULL, s);
				s->wait();
				delete s;
				//Now the bindings are effective for all tags
			}
		}

		//Update the displayList using the tags in this frame
		std::list<DisplayListTag*>::iterator it=blueprint.begin();
		for(;it!=blueprint.end();it++)
			(*it)->execute(parent, d);
		blueprint.clear();
		displayList=d;
		initialized=true;

		//Root movie clips are initialized now, after the first frame is really ready 
		if(parent->root==parent)
			parent->root->initialize();
		//Now the bindings are effective also for our parent (the root)
	}
}
