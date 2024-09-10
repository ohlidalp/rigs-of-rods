/// \title Engine diag script
/// \brief Shows config params and state of engine simulation



// #region FrameStep

// assume 60FPS = circa 3 sec
const int MAX_SAMPLES = 3*60;
array<float> clutchBuf(MAX_SAMPLES, 0.f);
array<float> rpmBuf(MAX_SAMPLES, 0.f);

void frameStep(float dt)
{
    ImGui::Begin("Engine Tool", true, 0);
    
    BeamClass@ playerVehicle = game.getCurrentTruck();
    if (@playerVehicle == null)
    {
        ImGui::Text("You are on foot.");
    }
    else
    {
        EngineSimClass@ engine = playerVehicle.getEngineSim();
        if (@engine == null)
        {
            ImGui::Text("Your vehicle doesn't have an engine");
        }
        else
        {
            clutchBuf.removeAt(0);            clutchBuf.insertLast(engine.getClutch()); 
            rpmBuf.removeAt(0);         rpmBuf.insertLast(engine.getRPM());
            drawEngineDiagUI(engine);
        }
    }
    
    ImGui::End();
}
// #endregion

// #region UI drawing
void drawEngineDiagUI(EngineSimClass@ engine)
{
    if (    ImGui::CollapsingHeader('engine args'))
    {
        ImGui::Columns(2);
        
        ImGui::TextDisabled("min RPM"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getMinRPM(), "", 0, 3));ImGui::NextColumn();        //float getMinRPM() const
        ImGui::TextDisabled("max RPM"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getMaxRPM(), "", 0, 3));ImGui::NextColumn();        //float getMaxRPM() const
        ImGui::TextDisabled("torque"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getEngineTorque(), "", 0, 3));ImGui::NextColumn();        //float getEngineTorque() const
        ImGui::TextDisabled("diff ratio"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getDiffRatio(), "", 0, 3));ImGui::NextColumn();        //float getDiffRatio() const
        
        
        ImGui::Columns(1);
        
        
        //float getGearRatio(int) const"
        //int getNumGears() const
        //int getNumGearsRanges() const
        ImGui::TextDisabled("gears (rev, neutral, "+engine.getNumGears()+" forward)");
        for (int i = -1; i <= engine.getNumGears(); i++)
        {
            ImGui::NextColumn();
            ImGui::Text(formatFloat(engine.getGearRatio(i), "", 0, 3));
        }
        
    }
    
    
    if (    ImGui::CollapsingHeader("engoption args"))
    {
        ImGui::Columns(2);
        
        ImGui::TextDisabled("inertia"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getEngineInertia(), "", 0, 3));         //float getEngineInertia()         
        ImGui::TextDisabled("type"); ImGui::NextColumn();        ImGui::Text(''+engine.getEngineType());ImGui::NextColumn();        //uint8 getEngineType()        
        ImGui::TextDisabled("is electric"); ImGui::NextColumn();        ImGui::Text(''+engine.isElectric());ImGui::NextColumn();        //bool isElectric()         
        ImGui::TextDisabled("has air"); ImGui::NextColumn();        ImGui::Text(''+engine.hasAir());ImGui::NextColumn();        //bool hasAir()         
        ImGui::TextDisabled("has turbo"); ImGui::NextColumn();        ImGui::Text(''+engine.hasTurbo());ImGui::NextColumn();        //bool hasTurbo()         
        ImGui::TextDisabled("clutch force"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getClutchForce(), "", 0, 3));ImGui::NextColumn();        //float getClutchForce()
        ImGui::TextDisabled("shift time"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getShiftTime(), "", 0, 3));ImGui::NextColumn();        //float getShiftTime()        
        ImGui::TextDisabled("clutch time"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getClutchTime(), "", 0, 3));ImGui::NextColumn();//float getClutchTime()         
        ImGui::TextDisabled("post shift time"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getPostShiftTime(), "", 0, 3));ImGui::NextColumn();//float getPostShiftTime()         
        ImGui::TextDisabled("stall RPM"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getStallRPM(), "", 0, 3));ImGui::NextColumn();//float getStallRPM() const        
        ImGui::TextDisabled("idle RPM"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getIdleRPM(), "", 0, 3));ImGui::NextColumn();//float getIdleRPM() const        
        ImGui::TextDisabled("max idle mixture"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getMaxIdleMixture(), "", 0, 3));ImGui::NextColumn();//float getMaxIdleMixture()        
        ImGui::TextDisabled("min idle mixture"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getMinIdleMixture(), "", 0, 3));ImGui::NextColumn();//float getMinIdleMixture()        
        ImGui::TextDisabled("braking torque"); ImGui::NextColumn();        ImGui::Text(formatFloat(engine.getBrakingTorque(), "", 0, 3));ImGui::NextColumn();//float getBrakingTorque() 
        ImGui::Columns(1);
    }
    
    if (ImGui::CollapsingHeader('state'))
    {
        ImGui::Columns(2);
        ImGui::TextDisabled("acc"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getAcc(), "", 0, 3));  ImGui::NextColumn();               //float          
        
        //DOC: "void PlotLines(const string&in label, array<float>&in values, int values_count, int values_offset = 0, const string&in overlay_text = string(), float scale_min = FLT_MAX, float scale_max = FLT_MAX, vector2 graph_size = vector2(0,0))",
        ImGui::TextDisabled("clutch (0.0 - 1.0)"); ImGui::NextColumn(); 
        ImGui::PlotLines("", clutchBuf, MAX_SAMPLES, 0, "", 0.f, 1.f); ImGui::SameLine(); 
        ImGui::Text(formatFloat(engine.getClutch(), "", 0, 3));  ImGui::NextColumn();            //float          
        
        ImGui::TextDisabled("crank factor"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getCrankFactor(), "", 0, 3)); ImGui::NextColumn();        //float          
        
        ImGui::TextDisabled("RPM (min - max)"); ImGui::NextColumn(); 
        ImGui::PlotLines("", rpmBuf, MAX_SAMPLES, 0, "", engine.getMinRPM(), engine.getMaxRPM()); ImGui::SameLine(); 
        ImGui::Text(formatFloat(engine.getRPM(), "", 0, 3));    ImGui::NextColumn();             //float          
        
        ImGui::TextDisabled("smoke"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getSmoke(), "", 0, 3));   ImGui::NextColumn();            //float          
        ImGui::TextDisabled("torque"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getTorque(), "", 0, 3));  ImGui::NextColumn();            //float          
        ImGui::TextDisabled("turbo PSI"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getTurboPSI(), "", 0, 3));  ImGui::NextColumn();          //float          
        ImGui::TextDisabled("auto mode"); ImGui::NextColumn(); ImGui::Text(''+engine.getAutoMode()  );  ImGui::NextColumn();        //SimGearboxMode 
        ImGui::TextDisabled("gear"); ImGui::NextColumn(); ImGui::Text(''+engine.getGear()      );  ImGui::NextColumn();        //int              
        ImGui::TextDisabled("gear range"); ImGui::NextColumn(); ImGui::Text(''+engine.getGearRange() );  ImGui::NextColumn();        //int              
        ImGui::TextDisabled("is running"); ImGui::NextColumn(); ImGui::Text(''+engine.isRunning()    );  ImGui::NextColumn();        //bool            
        ImGui::TextDisabled("has contact"); ImGui::NextColumn(); ImGui::Text(''+engine.hasContact()   );  ImGui::NextColumn();        //bool            
        ImGui::TextDisabled("cur engine torque"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getCurEngineTorque(), "", 0, 3));  ImGui::NextColumn();   //float          
        ImGui::TextDisabled("input shaft RPM"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getInputShaftRPM(), "", 0, 3));  ImGui::NextColumn();     //float          
        ImGui::TextDisabled("drive ratio"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getDriveRatio(), "", 0, 3));  ImGui::NextColumn();        //float          
        ImGui::TextDisabled("engine power"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getEnginePower(), "", 0, 3));  ImGui::NextColumn();       //float          
        //ImGui::TextDisabled(""); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getEnginePower(float)    //float          
        ImGui::TextDisabled("turbo power"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getTurboPower(), "", 0, 3));  ImGui::NextColumn();        //float          
        ImGui::TextDisabled("idle mix"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getIdleMixture(), "", 0, 3));  ImGui::NextColumn();       //float          
        ImGui::TextDisabled("prime mix"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getPrimeMixture(), "", 0, 3));  ImGui::NextColumn();      //float          
        ImGui::TextDisabled("auto shift"); ImGui::NextColumn(); ImGui::Text(''+engine.getAutoShift() );  ImGui::NextColumn();          //autoswitch     
        ImGui::TextDisabled("acc to hold RPM"); ImGui::NextColumn(); ImGui::Text(formatFloat(engine.getAccToHoldRPM() , "", 0, 3));  ImGui::NextColumn();     //float   
        ImGui::Columns(1);        
    }
}
