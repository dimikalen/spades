#pragma once

#include "../environment.hpp"
#include "../command.hpp"
#include "../command_type.hpp"
#include "../errors.hpp"
#include "../argument_list.hpp"

#include "drawing_command.hpp"

namespace online_visualization {
    class DrawPositionCommand : public DrawingCommand {
        private:
            void DrawPicture(Environment& curr_env, runtime_k::RtSeq kmer, string label = "") const {
                kmer = curr_env.kmer_mapper().Substitute(kmer);
                if (!curr_env.index().contains(kmer)) {
                    cout << "No corresponding graph location " << endl;
                    return;
                }
                pair<EdgeId, size_t> position = curr_env.index().get(kmer);
                if (position.second * 2 < curr_env.graph().length(position.first))
                    DrawingCommand::DrawPicture(curr_env, curr_env.graph().EdgeStart(position.first), label);
                else
                    DrawingCommand::DrawPicture(curr_env, curr_env.graph().EdgeEnd(position.first), label);
            }

        protected:
            size_t MinArgNumber() const {
                return 1;   
            }
            
            bool CheckCorrectness(const vector<string>& args) const {
                if (!CheckEnoughArguments(args))
                    return false;
                bool result = true;
                result = result & CheckIsNumber(args[0]);
                return result;
            }

        public:
            string Usage() const {
                string answer;
                answer = answer + "Command `draw_position` \n" + 
                                "Usage:\n" + 
                                "> position <position> [--rc] [-r]\n" + 
                                " You should specify an integer position in the genome, which location you want to look at. Optionally you can use a flag -r, whether you want the tool to invert the positions,\n" +
                                "and an option --rc, if you would like to see the pictures of the second strand.";
                return answer;
            }

            DrawPositionCommand() : DrawingCommand(CommandType::draw_position)
            {
            }

            void Execute(Environment& curr_env, const ArgumentList& arg_list) const {
                const vector<string>& args_ = arg_list.GetAllArguments();
                if (!CheckCorrectness(args_))
                    return;

                int position = GetInt(args_[0]);
                Sequence genome = curr_env.genome();
                if (arg_list["--rc"] == "true") {
                    cout << "Inverting genome...";
                    genome = !genome;
                }

                if (CheckPositionBounds(position, genome.size())) {
                    DrawPicture(curr_env, genome.Subseq(position).start<runtime_k::RtSeq::max_size>(cfg::get().K + 1), args_[0]);
                }

            }
    };
}
