## 09.02.2026
New function process_command_parametrized_test:                                                                                                       
  - Captures audio from microphone                                                                                                                      
  - Uses whisper general transcription (via existing transcribe function)                                                                               
  - Passes transcript to NLU for intent matching and parameter extraction                                                                               
  - Dispatches command with extracted parameters                                                                                                        
                                                                                                                                                        
  Mode selection flag:                                                                                                                                  
  static const bool k_use_parameterized_mode = true;                                                                                                    
  - When true: Uses NLU-based parameterized command processing (audio input)
  - When false: Falls back to original guided mode (requires commands.txt)                                                                              
                                                                                                                                                        
  Testing:                                                                                                                                              
  - File input mode (-if): Uses process_file_input with NLU                                                                                             
  - Audio mode (default with flag=true): Uses process_command_parametrized_test with NLU                                                                

  To test with real audio:                                                                                                                              
  ./build/Debug/examples/basic_example -m models/ggml-base.en.bin

  To test with file input (simulated whisper output):
  ./build/Debug/examples/basic_example -m models/ggml-base.en.bin -if examples/test_input.txt
  
    Currently running with params: -cmd /home/user/projects/voice_command/examples/commands.txt --input-file /home/user/projects/voice_command/examples/input_commands.txt
  
After the transcript is obtained:                                                                                                       
                                                                                                                                                        
  1. process_file_input: reads transcript from file → NLU → dispatch                                                                                    
  2. process_command_parametrized_test: gets transcript from whisper → NLU → dispatch
                                                                                                                                                        
  The post-transcript logic is duplicated in both, not a problem for now.
  



To check in current state:
Manual verification scenarios:
- Register command with required string param → dispatch without param → should fail
- Register command with optional param + default → dispatch without param → should inject default
- Register command with integer param (min=1, max=10) → dispatch with value 15 → should fail
- Register command with enum param → dispatch with invalid value → should fail


#
## 08.02.2026 Start
# Initial prompt
Hi, I need to develop a library which will allow users (other apps developers) easy add voice assistant into thier apps by 'commands' mechanism. The core of my library will be whisper.cpp and the commands example will be taken from here: https://github.com/ggml-org/whisper.cpp/tree/master/examples/command

By simply adding ICommand class with virtual 'execute' method (command design pattern) + CommandDispatcher + CommandRegistry I was able for example to add 'list all files' command. The execute implementation lists files in current directory. When command is recognized by whisper, it goes to CommandDispatcher, it finds and execute the overriden implementation.

Usage example: For example we have a raster map application and the developer wants to be able to create placemarks with voice. He will take my library, subclass ICommand, override 'execute' method to call 'CreatePlacemark' function from his application, register this subclass to registry and add a command string (for example 'AddPlacemark' to some json configuration file) . So when he will pronounce 'AddPlacemark', whisper will recognize it because it is in the list, then pass to dispatcher and he will find it and execute.

I need you to write a technical design document with implementation plan of such system. It should be detailed. The code will be written in c++17 according to google coding standards. It should be fast, with solid design, scalable and reasonable.

The most important part: how to handle such cases where user will say 'generate a linestring placemark with 10 vertices around New York city with dashed style and yellow color' ? As far as I understand we have to do something similar like tooling mechanism in LLMs (MCP) specifying special format for our 'tools' (commands). Or no? Tell me your thoughs about that, how this part should be done.


TODO in final state:
Separate the project to modular libs
1. Separate AudioCapture interface to other lib OR
2. Think about making voice_assistant lib to just accept interfaces and orchestrate them.
Using this, we will be able to keep the entire library non-qt and just provide a qt subclasses of audio capture
and voice assistant in separate 'head' library.
Make the voice assistant interface and provide a stl implementation
The idea: keep the lib free of qt.


The ICommand::Execute() runs on the processing thread, not the Qt UI thread. 


Verbose Registration (Minor Ergonomics)

  Registering a parameterized command requires 15-20 lines of boilerplate:
  - Create descriptor
  - Set name, description, triggers
  - Create each ParamDescriptor
  - Push to parameters vector
  - Create command instance
  - Register

  Recommendation: A builder pattern would improve ergonomics:
  registry->Register(
      CommandBuilder("set_color")
          .Description("Changes the rectangle color")
          .Triggers({"set color", "change color"})
          .EnumParam("color", {"red", "green", "blue"}, /*required=*/true)
          .Build(),
      std::make_unique<SetColorCommand>(widget));


Recommendations

  1. Document thread model explicitly - Add a section in design.md about which thread commands execute on and how Qt
  users should marshal to UI thread
  2. Consider CommandBuilder helper - Reduces boilerplate for command registration
  3. Optional: UI-thread execution mode for Qt - QtVoiceAssistant::SetExecuteOnUiThread(true) that uses
  QMetaObject::invokeMethod internally
  4. Optional: Lambda command support - For simple commands where a full class is overkill